
#include <Navigation.h>

#include "CHeaderSniffer.h"   // for ESaveFormat, which needs a better home.

#include "UCustomNavServicesDialogs.h"

const ResIDT kCustomSaveFilePanelDITLResID = 1550;
const Rect   kCustomSaveFilePanelSize = {0, 0, 48, 248};


enum
{
  eSaveFormatPanelStaticTextID     = 1,
  eSaveFormatPanelFormatPopupID
};

// ---------------------------------------------------------------------------
//	¥ CNavCallbackData
// ---------------------------------------------------------------------------
// generic callback data for any Nav Services extension
class CNavCallbackData
{
public:
                CNavCallbackData(const Rect& inPanelRect, ResIDT inPanelResID)
                : mPanelRect(inPanelRect)
                , mLastTryWidth(0)
                , mLastTryHeight(0)
                , mResource('DITL', inPanelResID, false /* don't throw */, false /* all res files */)
                , mFirstControlID(0)
                {
                }

      virtual   ~CNavCallbackData()
                {
                }
                
                
      Handle    GetDITLHandle() { return mResource.Get(); }
      
      void      SetFirstControlID(short inFirstID) { mFirstControlID = inFirstID; }
      short     GetFirstControlID() { return mFirstControlID; };    // only valid after the NavCustomControl call

  virtual void  NegotiatePanelBounds(NavCBRecPtr inNavCallbackData);
  
  virtual void  InitControls(NavCBRecPtr inNavCallbackData) = 0;
  virtual void  SaveControlValues(NavCBRecPtr inNavCallbackData) = 0;

protected:

  Rect          mPanelRect;
  short         mLastTryWidth;        // used during panel bounds negotiation
  short         mLastTryHeight;       // used during panel bounds negotiation
  StResource    mResource;
  short         mFirstControlID;
};


// ---------------------------------------------------------------------------
//	¥ NegotiatePanelBounds
// ---------------------------------------------------------------------------
void CNavCallbackData::NegotiatePanelBounds(NavCBRecPtr inNavCallbackData)
{
  Rect&   offeredRect   = inNavCallbackData->customRect;
  short   neededWidth   = offeredRect.left + (mPanelRect.right - mPanelRect.left);
  short   neededHeight  = offeredRect.top  + (mPanelRect.bottom - mPanelRect.top);

  // first round of negotiations
  if (offeredRect.right == 0 && offeredRect.bottom == 0)
  {
    // just tell it what dimensions we want
    offeredRect.right   = neededWidth;
    offeredRect.bottom  = neededHeight;
  }
  else
  {
    // we are in the middle of negotiating
    if (mLastTryWidth != offeredRect.right)
      if (offeredRect.right < neededWidth)
        offeredRect.right = neededWidth;

    if (mLastTryHeight != offeredRect.bottom)
      if (offeredRect.bottom < neededHeight)
        offeredRect.bottom = neededHeight;
        
    mLastTryWidth   = offeredRect.right;
    mLastTryHeight  = offeredRect.bottom;
  }  
}


#pragma mark -

// ---------------------------------------------------------------------------
//	¥ CNavCustomPutFileCallbackData
// ---------------------------------------------------------------------------
// callback data specific to our file saving dialog
class CNavCustomPutFileCallbackData : public CNavCallbackData
{
public:
                CNavCustomPutFileCallbackData(const Rect& inPanelRect, ResIDT inPanelResID, ESaveFormat inDefaultSaveFormat)
                : CNavCallbackData(inPanelRect, inPanelResID)
                , mSaveFormat(inDefaultSaveFormat)
                , mSaveFormatPopupControl(nil)
                {
                  if (mSaveFormat == eSaveFormatUnspecified)
                    mSaveFormat = eSaveFormatHTML;
                }

  virtual       ~CNavCustomPutFileCallbackData()
                {
                }
            
  virtual void  InitControls(NavCBRecPtr inNavCallbackData);
  virtual void  SaveControlValues(NavCBRecPtr inNavCallbackData);
  
  ESaveFormat   GetSaveFormat() { return mSaveFormat; }
  
protected:
  
  ESaveFormat   mSaveFormat;
  ControlHandle mSaveFormatPopupControl;
  
};


void CNavCustomPutFileCallbackData::InitControls(NavCBRecPtr inNavCallbackData)
{
  OSErr err = ::GetDialogItemAsControl(::GetDialogFromWindow(inNavCallbackData->window), mFirstControlID + eSaveFormatPanelFormatPopupID, &mSaveFormatPopupControl);
  ThrowIfOSErr_(err);
  
  ::SetControlValue(mSaveFormatPopupControl, (SInt32)mSaveFormat);
}


void CNavCustomPutFileCallbackData::SaveControlValues(NavCBRecPtr inNavCallbackData)
{
  if (mSaveFormatPopupControl)
    mSaveFormat = (ESaveFormat)::GetControlValue(mSaveFormatPopupControl);
}

#pragma mark -

// ---------------------------------------------------------------------------
//	¥ NavEventProc													  [static]
// ---------------------------------------------------------------------------
//	Event filter callback routine for Navigation Services

pascal void
UNavServicesDialogs::NavCustomEventProc(
	NavEventCallbackMessage		inSelector,
	NavCBRecPtr					      ioParams,
	NavCallBackUserData		    ioUserData)
{
  CNavCustomPutFileCallbackData *callbackData = reinterpret_cast<CNavCustomPutFileCallbackData*>(ioUserData);
  OSErr err = noErr;
  
	try			// Can't throw back through the Toolbox
	{
    switch (inSelector)
    {
      case kNavCBStart:
        if (callbackData)
        {
          err = ::NavCustomControl(ioParams->context, kNavCtlAddControlList, callbackData->GetDITLHandle());
          ThrowIfOSErr_(err);
          
          short firstControlID;
          err = ::NavCustomControl(ioParams->context, kNavCtlGetFirstControlID, &firstControlID);
          ThrowIfOSErr_(err);
          
          callbackData->SetFirstControlID(firstControlID);
          
          callbackData->InitControls(ioParams);
        }
        break;

      case kNavCBCustomize:
        if (callbackData)
        {
          callbackData->NegotiatePanelBounds(ioParams);
        }
        break;
        
      case kNavCBTerminate:
        if (callbackData)
        {
          callbackData->SaveControlValues(ioParams);
        }
        break;  
    
      case kNavCBEvent:
        {
          short itemHit = ioParams->eventData.itemHit;
        
          if (itemHit == eSaveFormatPanelFormatPopupID)
          {
            // process control click?
            break;
          }
   			  UModalAlerts::ProcessModalEvent(*(ioParams->eventData.eventDataParms.event));
   			}
        break;
    }
  }
  catch (...)
  {
    Assert_(0); // error
  }  
}


// ---------------------------------------------------------------------------
//	¥ LCustomFileDesignator
// ---------------------------------------------------------------------------

UNavServicesDialogs::LCustomFileDesignator::LCustomFileDesignator()
{

}

// ---------------------------------------------------------------------------
//	¥ ~LCustomFileDesignator
// ---------------------------------------------------------------------------
UNavServicesDialogs::LCustomFileDesignator::~LCustomFileDesignator()
{
}

// ---------------------------------------------------------------------------
//	¥ LCustomFileDesignator::AskDesignateFile								  [public]
// ---------------------------------------------------------------------------

bool
UNavServicesDialogs::LCustomFileDesignator::AskDesignateFile(ConstStringPtr inDefaultName, ESaveFormat& ioSaveFormat)
{
	StNavEventUPP		eventUPP(NavCustomEventProc);

  CNavCustomPutFileCallbackData callbackData(kCustomSaveFilePanelSize, kCustomSaveFilePanelDITLResID, ioSaveFormat);
  
	LString::CopyPStr(inDefaultName, mNavOptions.savedFileName);

	mNavReply.SetDefaultValues();

	AEDesc*		defaultLocationDesc = nil;
	if (not mDefaultLocation.IsNull()) {
		defaultLocationDesc = mDefaultLocation;

		if (mSelectDefault) {
			mNavOptions.dialogOptionFlags |= kNavSelectDefaultLocation;
		} else {
			mNavOptions.dialogOptionFlags &= ~kNavSelectDefaultLocation;
		}
	}

	UDesktop::Deactivate();

	OSErr err = ::NavPutFile(
						defaultLocationDesc,
						mNavReply,
						&mNavOptions,
						eventUPP,
						mFileType,
						mFileCreator,
						&callbackData);

	UDesktop::Activate();

	if ( (err != noErr) && (err != userCanceledErr) ) {
		Throw_(err);
	}

  ioSaveFormat = callbackData.GetSaveFormat();
	return mNavReply.IsValid();
}

