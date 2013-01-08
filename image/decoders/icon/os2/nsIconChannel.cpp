/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//------------------------------------------------------------------------

#include "nsIconChannel.h"
#include "nsIIconURI.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsIFile.h"
#include "nsIFileURL.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIRwsService.h"

#define INCL_PM
#include <os2.h>

//------------------------------------------------------------------------

// Due to byte swap, the second nibble is the first pixel of the pair
#define FIRSTPEL(x)  (0xF & (x >> 4))
#define SECONDPEL(x) (0xF & x)

// nbr of bytes per row, rounded up to the nearest dword boundary
#define ALIGNEDBPR(cx,bits) ((( ((cx)*(bits)) + 31) / 32) * 4)

// nbr of bytes per row, rounded up to the nearest byte boundary
#define UNALIGNEDBPR(cx,bits) (( ((cx)*(bits)) + 7) / 8)

// native icon functions
static  HPOINTER GetIcon(nsCString& file, bool fExists,
                         bool fMini, bool *fWpsIcon);
static  void DestroyIcon(HPOINTER hIcon, bool fWpsIcon);

void    ConvertColorBitMap(uint8_t *inBuf, PBITMAPINFO2 pBMInfo,
                           uint8_t *outBuf, bool fShrink);
void    ConvertMaskBitMap(uint8_t *inBuf, PBITMAPINFO2 pBMInfo,
                          uint8_t *outBuf, bool fShrink);

//------------------------------------------------------------------------

// reduces overhead by preventing calls to nsRws when it isn't present
static bool sUseRws = true;

//------------------------------------------------------------------------
// nsIconChannel methods

nsIconChannel::nsIconChannel()
{
}

nsIconChannel::~nsIconChannel()
{}

NS_IMPL_THREADSAFE_ISUPPORTS4(nsIconChannel, 
                              nsIChannel,
                              nsIRequest,
                              nsIRequestObserver,
                              nsIStreamListener)

nsresult nsIconChannel::Init(nsIURI* uri)
{
  NS_ASSERTION(uri, "no uri");
  mUrl = uri;
  mOriginalURI = uri;
  nsresult rv;
  mPump = do_CreateInstance(NS_INPUTSTREAMPUMP_CONTRACTID, &rv);
  return rv;
}

//------------------------------------------------------------------------
// nsIRequest methods:

NS_IMETHODIMP nsIconChannel::GetName(nsACString &result)
{
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsIconChannel::IsPending(bool *result)
{
  return mPump->IsPending(result);
}

NS_IMETHODIMP nsIconChannel::GetStatus(nsresult *status)
{
  return mPump->GetStatus(status);
}

NS_IMETHODIMP nsIconChannel::Cancel(nsresult status)
{
  return mPump->Cancel(status);
}

NS_IMETHODIMP nsIconChannel::Suspend(void)
{
  return mPump->Suspend();
}

NS_IMETHODIMP nsIconChannel::Resume(void)
{
  return mPump->Resume();
}

NS_IMETHODIMP nsIconChannel::GetLoadGroup(nsILoadGroup* *aLoadGroup)
{
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetLoadGroup(nsILoadGroup* aLoadGroup)
{
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetLoadFlags(uint32_t *aLoadAttributes)
{
  return mPump->GetLoadFlags(aLoadAttributes);
}

NS_IMETHODIMP nsIconChannel::SetLoadFlags(uint32_t aLoadAttributes)
{
  return mPump->SetLoadFlags(aLoadAttributes);
}

//------------------------------------------------------------------------
// nsIChannel methods:

NS_IMETHODIMP nsIconChannel::GetOriginalURI(nsIURI* *aURI)
{
  *aURI = mOriginalURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::Open(nsIInputStream **_retval)
{
  return MakeInputStream(_retval, false);
}

NS_IMETHODIMP nsIconChannel::AsyncOpen(nsIStreamListener *aListener, nsISupports *ctxt)
{
  nsCOMPtr<nsIInputStream> inStream;
  nsresult rv = MakeInputStream(getter_AddRefs(inStream), true);
  if (NS_FAILED(rv))
    return rv;

  // Init our streampump
  rv = mPump->Init(inStream, int64_t(-1), int64_t(-1), 0, 0, false);
  if (NS_FAILED(rv))
    return rv;

  rv = mPump->AsyncRead(this, ctxt);
  if (NS_SUCCEEDED(rv)) {
    // Store our real listener
    mListener = aListener;
    // Add ourself to the load group, if available
    if (mLoadGroup)
      mLoadGroup->AddRequest(this, nullptr);
  }
  return rv;
}

nsresult nsIconChannel::ExtractIconInfoFromUrl(nsIFile ** aLocalFile, uint32_t * aDesiredImageSize, nsACString &aContentType, nsACString &aFileExtension)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMozIconURI> iconURI (do_QueryInterface(mUrl, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  iconURI->GetImageSize(aDesiredImageSize);
  iconURI->GetContentType(aContentType);
  iconURI->GetFileExtension(aFileExtension);

  nsCOMPtr<nsIURL> url;
  rv = iconURI->GetIconURL(getter_AddRefs(url));
  if (NS_FAILED(rv) || !url) return NS_OK;

  nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(url, &rv);
  if (NS_FAILED(rv) || !fileURL) return NS_OK;

  nsCOMPtr<nsIFile> file;
  rv = fileURL->GetFile(getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) return NS_OK;

  *aLocalFile = file;
  NS_IF_ADDREF(*aLocalFile);
  return NS_OK;
}

//------------------------------------------------------------------------

// retrieves a native icon with 16, 256, or 16M colors and converts it to cairo
// format;  Note: this implementation ignores the file's MIME-type because
// using it virtually guarantees we'll end up with an inappropriate icon (i.e.
// an .exe icon)

nsresult nsIconChannel::MakeInputStream(nsIInputStream **_retval,
                                        bool nonBlocking)
{

  // get some details about this icon
  nsCOMPtr<nsIFile> localFile;
  uint32_t desiredImageSize;
  nsXPIDLCString contentType;
  nsAutoCString filePath;
  nsresult rv = ExtractIconInfoFromUrl(getter_AddRefs(localFile),
                                       &desiredImageSize, contentType,
                                       filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  // if the file exists, get its path
  bool fileExists = false;
  if (localFile) {
    localFile->GetNativePath(filePath);
    localFile->Exists(&fileExists);
  }

  // get the file's icon from either the WPS or PM
  bool fWpsIcon = false;
  HPOINTER hIcon = GetIcon(filePath, fileExists,
                           desiredImageSize <= 16, &fWpsIcon);
  if (hIcon == NULLHANDLE)
    return NS_ERROR_FAILURE;

  // get the color & mask bitmaps used by the icon
  POINTERINFO IconInfo;
  if (!WinQueryPointerInfo(hIcon, &IconInfo)) {
    DestroyIcon(hIcon, fWpsIcon);
    return NS_ERROR_FAILURE;
  }

  // if we need a mini-icon, use those bitmaps if present;
  // otherwise, signal that the icon needs to be shrunk
  bool fShrink = FALSE;
  if (desiredImageSize <= 16) {
    if (IconInfo.hbmMiniPointer) {
      IconInfo.hbmColor = IconInfo.hbmMiniColor;
      IconInfo.hbmPointer = IconInfo.hbmMiniPointer;
    } else {
      fShrink = TRUE;
    }
  }

  // various resources to be allocated
  PBITMAPINFO2  pBMInfo = 0;
  uint8_t*      pInBuf  = 0;
  uint8_t*      pOutBuf = 0;
  HDC           hdc     = 0;
  HPS           hps     = 0;

  // using this dummy do{...}while(0) "loop" guarantees that resources will
  // be deallocated, but eliminates the need for nesting, and generally makes
  // testing for & dealing with errors pretty painless (just 'break')
  do {
    rv = NS_ERROR_FAILURE;

    // get the details for the color bitmap;  if there isn't one
    // or this is 1-bit color, exit
    BITMAPINFOHEADER2  BMHeader;
    BMHeader.cbFix = sizeof(BMHeader);
    if (!IconInfo.hbmColor ||
        !GpiQueryBitmapInfoHeader(IconInfo.hbmColor, &BMHeader) ||
        BMHeader.cBitCount == 1)
      break;

    // alloc space for the color bitmap's info, including its color table
    uint32_t cbBMInfo = sizeof(BITMAPINFO2) + (sizeof(RGB2) * 255);
    pBMInfo = (PBITMAPINFO2)nsMemory::Alloc(cbBMInfo);
    if (!pBMInfo)
      break;

    // alloc space for the color bitmap data
    uint32_t cbInRow = ALIGNEDBPR(BMHeader.cx, BMHeader.cBitCount);
    uint32_t cbInBuf = cbInRow * BMHeader.cy;
    pInBuf = (uint8_t*)nsMemory::Alloc(cbInBuf);
    if (!pInBuf)
      break;
    memset(pInBuf, 0, cbInBuf);

    // alloc space for the BGRA32 bitmap we're creating
    uint32_t cxOut    = fShrink ? BMHeader.cx / 2 : BMHeader.cx;
    uint32_t cyOut    = fShrink ? BMHeader.cy / 2 : BMHeader.cy;
    uint32_t cbOutBuf = 2 + ALIGNEDBPR(cxOut, 32) * cyOut;
    pOutBuf = (uint8_t*)nsMemory::Alloc(cbOutBuf);
    if (!pOutBuf)
      break;
    memset(pOutBuf, 0, cbOutBuf);

    // create a DC and PS
    DEVOPENSTRUC dop = {NULL, "DISPLAY", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    hdc = DevOpenDC((HAB)0, OD_MEMORY, "*", 5L, (PDEVOPENDATA)&dop, NULLHANDLE);
    if (!hdc)
      break;

    SIZEL sizel = {0,0};
    hps = GpiCreatePS((HAB)0, hdc, &sizel, GPIA_ASSOC | PU_PELS | GPIT_MICRO);
    if (!hps)
      break;

    // get the color bits
    memset(pBMInfo, 0, cbBMInfo);
    *((PBITMAPINFOHEADER2)pBMInfo) = BMHeader;
    GpiSetBitmap(hps, IconInfo.hbmColor);
    if (GpiQueryBitmapBits(hps, 0L, (LONG)BMHeader.cy,
                           (BYTE*)pInBuf, pBMInfo) <= 0)
      break;

    // The first 2 bytes are the width & height of the icon in pixels,
    // the remaining bytes are BGRA32 (B in first byte, A in last)
    uint8_t* outPtr = pOutBuf;
    *outPtr++ = (uint8_t)cxOut;
    *outPtr++ = (uint8_t)cyOut;

    // convert the color bitmap
    pBMInfo->cbImage = cbInBuf;
    ConvertColorBitMap(pInBuf, pBMInfo, outPtr, fShrink);

    // now we need to tack on the alpha data, so jump back to the first
    // pixel in the output buffer
    outPtr = pOutBuf+2;

    // Get the mask info
    BMHeader.cbFix = sizeof(BMHeader);
    if (!GpiQueryBitmapInfoHeader(IconInfo.hbmPointer, &BMHeader))
      break;

    // if the existing input buffer isn't large enough, reallocate it
    cbInRow = ALIGNEDBPR(BMHeader.cx, BMHeader.cBitCount);
    if ((cbInRow * BMHeader.cy) > cbInBuf)  // Need more for mask
    {
      cbInBuf = cbInRow * BMHeader.cy;
      nsMemory::Free(pInBuf);
      pInBuf = (uint8_t*)nsMemory::Alloc(cbInBuf);
      memset(pInBuf, 0, cbInBuf);
    }

    // get the mask/alpha bits
    memset(pBMInfo, 0, cbBMInfo);
    *((PBITMAPINFOHEADER2)pBMInfo) = BMHeader;
    GpiSetBitmap(hps, IconInfo.hbmPointer);
    if (GpiQueryBitmapBits(hps, 0L, (LONG)BMHeader.cy,
                           (BYTE*)pInBuf, pBMInfo) <= 0)
      break;

    // convert the mask/alpha bitmap
    pBMInfo->cbImage = cbInBuf;
    ConvertMaskBitMap(pInBuf, pBMInfo, outPtr, fShrink);

    // create a pipe
    nsCOMPtr<nsIInputStream> inStream;
    nsCOMPtr<nsIOutputStream> outStream;
    rv = NS_NewPipe(getter_AddRefs(inStream), getter_AddRefs(outStream),
                    cbOutBuf, cbOutBuf, nonBlocking);
    if (NS_FAILED(rv))
      break;

    // put our data into the pipe
    uint32_t written;
    rv = outStream->Write(reinterpret_cast<const char*>(pOutBuf),
                          cbOutBuf, &written);
    if (NS_FAILED(rv))
      break;

    // success! so addref the pipe
    NS_ADDREF(*_retval = inStream);
  } while (0);

  // free all the resources we allocated
  if (pOutBuf)
    nsMemory::Free(pOutBuf);
  if (pInBuf)
    nsMemory::Free(pInBuf);
  if (pBMInfo)
    nsMemory::Free(pBMInfo);
  if (hps) {
    GpiAssociate(hps, NULLHANDLE);
    GpiDestroyPS(hps);
  }
  if (hdc)
    DevCloseDC(hdc);
  if (hIcon)
    DestroyIcon(hIcon, fWpsIcon);

  return rv;
}

//------------------------------------------------------------------------

// get the file's icon from either the WPS or PM

static HPOINTER GetIcon(nsCString& file, bool fExists,
                        bool fMini, bool *fWpsIcon)
{
  HPOINTER hRtn = 0;
  *fWpsIcon = false;

  if (file.IsEmpty()) {
    // append something so that we get at least the generic icon
    file.Append("pmwrlw");
  }

  // if RWS is enabled, try to get the icon from the WPS
  if (sUseRws) {
    nsCOMPtr<nsIRwsService> rwsSvc(do_GetService("@mozilla.org/rwsos2;1"));
    if (!rwsSvc)
      sUseRws = false;
    else {
      if (fExists) {
        rwsSvc->IconFromPath(file.get(), false, fMini, (uint32_t*)&hRtn);
      } else {
        const char *ptr = file.get();
        if (*ptr == '.')
          ptr++;
        rwsSvc->IconFromExtension(ptr, fMini, (uint32_t*)&hRtn);
      }
    }
  }

  // if we got an icon from the WPS, set the flag & exit
  if (hRtn) {
    *fWpsIcon = true;
    return hRtn;
  }

  // if the file exists already, get its icon
  if (fExists)
    return WinLoadFileIcon(file.get(), FALSE);

  // otherwise, create a temporary file with the correct extension,
  // then retrieve whatever icon PM assigns it
  if (file.First() == '.')
    file.Insert("moztmp", 0);

  nsCOMPtr<nsIFile> tempPath;
  if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tempPath))) ||
      NS_FAILED(tempPath->AppendNative(file)))
    return 0;

  nsAutoCString pathStr;
  tempPath->GetNativePath(pathStr);
  FILE* fp = fopen(pathStr.get(), "wb+");
  if (fp) {
    fclose(fp);
    hRtn = WinLoadFileIcon(pathStr.get(), FALSE);
    remove(pathStr.get());
  }

  return hRtn;
}

//------------------------------------------------------------------------

static void DestroyIcon(HPOINTER hIcon, bool fWpsIcon)
{
  if (fWpsIcon)
    WinDestroyPointer(hIcon);
  else
    WinFreeFileIcon(hIcon);

  return;
}

//------------------------------------------------------------------------

// converts 16, 256, & 16M color bitmaps to BGRA32 format;  the alpha
// channel byte is left open & will be filled in by ConvertMaskBitMap();
// since the scanlines in OS/2 bitmaps run bottom-to-top, it starts at the
// end of the input buffer & works its way back;  Note: only 4-bit, 20x20
// icons contain input padding that has to be ignored

void ConvertColorBitMap(uint8_t *inBuf, PBITMAPINFO2 pBMInfo,
                        uint8_t *outBuf, bool fShrink)
{
  uint32_t next  = fShrink ? 2 : 1;
  uint32_t bprIn = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  uint8_t *pIn   = inBuf + (pBMInfo->cy - 1) * bprIn;
  uint8_t *pOut  = outBuf;
  PRGB2    pColorTable = &pBMInfo->argbColor[0];

  if (pBMInfo->cBitCount == 4) {
    uint32_t  ubprIn = UNALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
    uint32_t  padIn  = bprIn - ubprIn;

    for (uint32_t row = pBMInfo->cy; row > 0; row -= next) {
      for (uint32_t ndx = 0; ndx < ubprIn; ndx++, pIn++) {
        pOut = 4 + (uint8_t*)memcpy(pOut, &pColorTable[FIRSTPEL(*pIn)], 3);
        if (!fShrink) {
          pOut = 4 + (uint8_t*)memcpy(pOut, &pColorTable[SECONDPEL(*pIn)], 3);
        }
      }
      pIn -= ((next + 1) * bprIn) - padIn;
    }
  } else if (pBMInfo->cBitCount == 8) {
    for (uint32_t row = pBMInfo->cy; row > 0; row -= next) {
      for (uint32_t ndx = 0; ndx < bprIn; ndx += next, pIn += next) {
        pOut = 4 + (uint8_t*)memcpy(pOut, &pColorTable[*pIn], 3);
      }
      pIn -= (next + 1) * bprIn;
    }
  } else if (pBMInfo->cBitCount == 24) {
    uint32_t next3 = next * 3;
    for (uint32_t row = pBMInfo->cy; row > 0; row -= next) {
      for (uint32_t ndx = 0; ndx < bprIn; ndx += next3, pIn += next3) {
        pOut = 4 + (uint8_t*)memcpy(pOut, pIn, 3);
      }
      pIn -= (next + 1) * bprIn;
    }
  }
}

//------------------------------------------------------------------------

// converts an icon's AND mask into 8-bit alpha data and stores it in the
// high-order bytes of the BGRA32 bitmap created by ConvertColorBitMap();
// the AND mask is the 2nd half of a pair of bitmaps & the scanlines run
// bottom-to-top - starting at the end and working back to the midpoint
// converts the entire bitmap;  since each row is dword-aligned, 16, 20,
// and 40-pixel icons all have padding bits at the end of every row that
// must be skipped over

void ConvertMaskBitMap(uint8_t *inBuf, PBITMAPINFO2 pBMInfo,
                       uint8_t *outBuf, bool fShrink)
{
  uint32_t next   = (fShrink ? 2 : 1);
  uint32_t bprIn  = ALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  uint32_t padIn  = bprIn - UNALIGNEDBPR(pBMInfo->cx, pBMInfo->cBitCount);
  uint8_t *pIn    = inBuf + (pBMInfo->cy - 1) * bprIn;
  uint8_t *pOut   = outBuf + 3;

  // for each row or every other row
  for (uint32_t row = pBMInfo->cy/2; row > 0; row -= next) {

    // for all of the non-padding bits in the row
    for (uint32_t bits = pBMInfo->cx; bits; pIn++) {
      uint8_t src = ~(*pIn);
      uint8_t srcMask = 0x80;

      // for each bit or every other bit in the current byte
      for ( ; srcMask && bits; srcMask >>= next, bits -= next, pOut += 4) {
        if (src & srcMask) {
          *pOut = 0xff;
        }
      }
    }

    // if the row just completed had padding, pIn won't be pointing
    // at the first byte in the next row;  padIn compensates for this
    pIn -= ((next + 1) * bprIn) - padIn;
  }
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsIconChannel::GetContentType(nsACString &aContentType) 
{
  aContentType.AssignLiteral(IMAGE_ICON_MS);
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentType(const nsACString &aContentType)
{
  // It doesn't make sense to set the content-type on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsIconChannel::GetContentCharset(nsACString &aContentCharset) 
{
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsIconChannel::SetContentCharset(const nsACString &aContentCharset)
{
  // It doesn't make sense to set the content-charset on this type
  // of channel...
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionFilename(nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::SetContentDispositionFilename(const nsAString &aContentDispositionFilename)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsIconChannel::GetContentDispositionHeader(nsACString &aContentDispositionHeader)
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsIconChannel::GetContentLength(int64_t *aContentLength)
{
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetContentLength(int64_t aContentLength)
{
  NS_NOTREACHED("nsIconChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsIconChannel::GetOwner(nsISupports* *aOwner)
{
  *aOwner = mOwner.get();
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetOwner(nsISupports* aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  *aNotificationCallbacks = mCallbacks.get();
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nullptr;
  return NS_OK;
}

// nsIRequestObserver methods
NS_IMETHODIMP nsIconChannel::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  if (mListener)
    return mListener->OnStartRequest(this, aContext);
  return NS_OK;
}

NS_IMETHODIMP nsIconChannel::OnStopRequest(nsIRequest* aRequest, nsISupports* aContext, nsresult aStatus)
{
  if (mListener) {
    mListener->OnStopRequest(this, aContext, aStatus);
    mListener = nullptr;
  }

  // Remove from load group
  if (mLoadGroup)
    mLoadGroup->RemoveRequest(this, nullptr, aStatus);

  // Drop notification callbacks to prevent cycles.
  mCallbacks = nullptr;

  return NS_OK;
}

//------------------------------------------------------------------------
// nsIStreamListener methods
NS_IMETHODIMP nsIconChannel::OnDataAvailable(nsIRequest* aRequest,
                                             nsISupports* aContext,
                                             nsIInputStream* aStream,
                                             uint64_t aOffset,
                                             uint32_t aCount)
{
  if (mListener)
    return mListener->OnDataAvailable(this, aContext, aStream, aOffset, aCount);
  return NS_OK;
}

//------------------------------------------------------------------------

