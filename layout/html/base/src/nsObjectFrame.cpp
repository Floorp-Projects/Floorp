/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsHTMLParts.h"
#include "nsLeafFrame.h"
#include "nsCSSLayout.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsString.h"
#include "nsIContentViewerContainer.h"
#include "prmem.h"
#include "nsHTMLAtoms.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIHTMLContent.h"
#include "nsISupportsArray.h"
#include "plstr.h"
#include "nsILinkHandler.h"

// XXX For temporary paint code
#include "nsIStyleContext.h"

class nsObjectFrame;

class nsPluginInstanceOwner : public nsIPluginInstanceOwner {
public:
  nsPluginInstanceOwner();
  ~nsPluginInstanceOwner();

  NS_DECL_ISUPPORTS

  //nsIPluginInstanceOwner interface

  NS_IMETHOD SetInstance(nsIPluginInstance *aInstance);

  NS_IMETHOD GetInstance(nsIPluginInstance *&aInstance);

  NS_IMETHOD GetWindow(nsPluginWindow *&aWindow);

  NS_IMETHOD GetMode(nsPluginMode *aMode);

  NS_IMETHOD GetAttributes(PRUint16& n, const char*const*& names,
                           const char*const*& values);

  NS_IMETHOD GetAttribute(const char* name, const char* *result);

  NS_IMETHOD CreateWidget(void);

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget, void *aPostData);

  //locals

  NS_IMETHOD Init(nsIPresContext *aPresContext, nsObjectFrame *aFrame);

private:
  nsPluginWindow    mPluginWindow;
  nsIPluginInstance *mInstance;
  nsObjectFrame     *mOwner;
  PRInt32           mNumAttrs;
  char              **mAttrNames;
  char              **mAttrVals;
  nsIWidget         *mWidget;
  nsIPresContext    *mContext;
};

#define nsObjectFrameSuper nsLeafFrame

class nsObjectFrame : public nsObjectFrameSuper {
public:
  nsObjectFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);
  NS_IMETHOD Scrolled(nsIView *aView);

  //local methods
  nsresult CreateWidget(nscoord aWidth, nscoord aHeight, PRBool aViewOnly);
  nsresult GetFullURL(nsString& aFullURL);

protected:
  virtual ~nsObjectFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);


  nsresult SetURL(const nsString& aURLSpec);
  nsresult SetBaseHREF(const nsString& aBaseHREF);
  nsresult SetFullURL(const nsString& aURLSpec);

private:
  nsPluginInstanceOwner *mInstanceOwner;
  nsString              *mURLSpec;
  nsString              *mBaseHREF;
  nsString              *mFullURL;
};

nsObjectFrame::nsObjectFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsObjectFrameSuper(aContent, aParentFrame)
{
}

nsObjectFrame::~nsObjectFrame()
{
  NS_IF_RELEASE(mInstanceOwner);

  if (nsnull != mURLSpec)
  {
    delete mURLSpec;
    mURLSpec = nsnull;
  }

  if (nsnull != mBaseHREF)
  {
    delete mBaseHREF;
    mBaseHREF = nsnull;
  }

  if (nsnull != mFullURL)
  {
    delete mFullURL;
    mFullURL = nsnull;
  }
}

static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);

nsresult
nsObjectFrame::CreateWidget(nscoord aWidth, nscoord aHeight, PRBool aViewOnly)
{
  nsIView* view;

  // Create our view and widget

  nsresult result = 
    nsRepository::CreateInstance(kViewCID, nsnull, kIViewIID,
				 (void **)&view);
  if (NS_OK != result) {
    return result;
  }
  nsIViewManager *viewMan;    // need to release

  nsRect boundBox(0, 0, aWidth, aHeight); 

  nsIFrame* parWithView;
  nsIView *parView;

  GetParentWithView(parWithView);
  parWithView->GetView(parView);

  if (NS_OK == parView->GetViewManager(viewMan))
  {
  //  nsWidgetInitData* initData = GetWidgetInitData(*aPresContext); // needs to be deleted
    // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, &kWidgetCID, nsnull,
                        nsnull, 0, nsnull,
                        1.0f, nsViewVisibility_kHide);
  //  if (nsnull != initData) {
  //    delete(initData);
  //  }
    if (NS_OK != result) {
      result = NS_OK;       //XXX why OK? MMP
      goto exit;            //XXX sue me. MMP
    }

  #if 0
    // set the content's widget, so it can get content modified by the widget
    nsIWidget *widget;
    result = GetWidget(view, &widget);
    if (NS_OK == result) {
      nsInput* content = (nsInput *)mContent; // change this cast to QueryInterface 
      content->SetWidget(widget);
      NS_IF_RELEASE(widget);
    } else {
      NS_ASSERTION(0, "could not get widget");
    }
  #endif

    viewMan->InsertChild(parView, view, 0);
  }

  {
    //this is ugly. it was ripped off from didreflow(). MMP
    // Position and size view relative to its parent, not relative to our
    // parent frame (our parent frame may not have a view).

    nsIView* parentWithView;
    nsPoint origin;
    view->SetVisibility(nsViewVisibility_kShow);
    GetOffsetFromView(origin, parentWithView);
    viewMan->ResizeView(view, mRect.width, mRect.height);
    viewMan->MoveViewTo(view, origin.x, origin.y);
  }

  SetView(view);

exit:
  NS_IF_RELEASE(viewMan);  

  return result;
}

#define EMBED_DEF_DIM 50

void
nsObjectFrame::GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aMetrics)
{
  // Determine our size stylistically
  nsSize styleSize;
  PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState, styleSize);
  PRBool haveWidth = PR_FALSE;
  PRBool haveHeight = PR_FALSE;
  if (0 != (ss & NS_SIZE_HAS_WIDTH)) {
    aMetrics.width = styleSize.width;
    haveWidth = PR_TRUE;
  }
  if (0 != (ss & NS_SIZE_HAS_HEIGHT)) {
    aMetrics.height = styleSize.height;
    haveHeight = PR_TRUE;
  }

  // XXX Temporary auto-sizing logic
  if (!haveWidth) {
    if (haveHeight) {
      aMetrics.width = aMetrics.height;
    }
    else {
      float p2t = aPresContext->GetPixelsToTwips();
      aMetrics.width = NSIntPixelsToTwips(EMBED_DEF_DIM, p2t);
    }
  }
  if (!haveHeight) {
    if (haveWidth) {
      aMetrics.height = aMetrics.width;
    }
    else {
      float p2t = aPresContext->GetPixelsToTwips();
      aMetrics.height = NSIntPixelsToTwips(EMBED_DEF_DIM, p2t);
    }
  }
  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;
}

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext&      aPresContext,
                      nsReflowMetrics&     aMetrics,
                      const nsReflowState& aReflowState,
                      nsReflowStatus&      aStatus)
{
  // Get our desired size
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  // XXX deal with border and padding the usual way...wrap it up!

  nsIAtom* atom;
  mContent->GetTag(atom);
  if ((nsnull != atom) && (nsnull == mInstanceOwner)) {
    //don't make a view for an applet since we know we can't support them yet...
    if (atom != nsHTMLAtoms::applet) {
      static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
      static NS_DEFINE_IID(kIContentViewerContainerIID, NS_ICONTENT_VIEWER_CONTAINER_IID);

      nsISupports               *container;
      nsIPluginHost             *pm;
      nsIContentViewerContainer *cv;
      nsresult                  rv;

      mInstanceOwner = new nsPluginInstanceOwner();

      if (nsnull != mInstanceOwner) {
        NS_ADDREF(mInstanceOwner);
        mInstanceOwner->Init(&aPresContext, this);

        rv = aPresContext.GetContainer(&container);

        if (NS_OK == rv) {
          rv = container->QueryInterface(kIContentViewerContainerIID, (void **)&cv);

          if (NS_OK == rv) {
            rv = cv->QueryCapability(kIPluginHostIID, (void **)&pm);

            if (NS_OK == rv) {
              nsAutoString    type;
              char            *buf = nsnull;
              PRInt32         buflen;
              nsPluginWindow  *window;
              float           t2p = aPresContext.GetTwipsToPixels();

              mInstanceOwner->GetWindow(window);

              mContent->GetAttribute(nsString("type"), type);

              buflen = type.Length();

              if (buflen > 0) {
                buf = (char *)PR_Malloc(buflen + 1);

                if (nsnull != buf)
                  type.ToCString(buf, buflen + 1);
              }

              nsAutoString src, base, fullurl;

              //stream in the object source if there is one...

              if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute("SRC", src)) {
                SetURL(src);

                if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(NS_HTML_BASE_HREF, base))
                  SetBaseHREF(base);

                nsIPresShell  *shell = aPresContext.GetShell();
                nsIDocument   *doc = shell->GetDocument();
                nsIURL        *docURL = doc->GetDocumentURL();

                // Create an absolute URL
                nsresult rv = NS_MakeAbsoluteURL(docURL, base, *mURLSpec, fullurl);

                SetFullURL(fullurl);

                NS_RELEASE(shell);
                NS_RELEASE(docURL);
                NS_RELEASE(doc);
              }

              nsIView *parentWithView;
              nsPoint origin;

              GetOffsetFromView(origin, parentWithView);

              window->x = NSTwipsToIntPixels(origin.x, t2p);
              window->y = NSTwipsToIntPixels(origin.y, t2p);
              window->width = NSTwipsToIntPixels(aMetrics.width, t2p);
              window->height = NSTwipsToIntPixels(aMetrics.height, t2p);
              window->clipRect.top = 0;
              window->clipRect.left = 0;
//              window->clipRect.top = NSTwipsToIntPixels(origin.y, t2p);
//              window->clipRect.left = NSTwipsToIntPixels(origin.x, t2p);
              window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
              window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
#ifdef XP_UNIX
              window->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif
              rv = pm->InstantiatePlugin(buf, fullurl, mInstanceOwner);

              PR_Free((void *)buf);

              NS_RELEASE(pm);
            }
            NS_RELEASE(cv);
          }
          NS_RELEASE(container);
        }
      }
    }
    NS_RELEASE(atom);
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

NS_IMETHODIMP
nsObjectFrame::DidReflow(nsIPresContext& aPresContext,
                         nsDidReflowStatus aStatus)
{
  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(view);
    if (nsnull != view) {
      view->SetVisibility(nsViewVisibility_kShow);
    }

    if (nsnull != mInstanceOwner) {
      nsPluginWindow    *window;

      if (NS_OK == mInstanceOwner->GetWindow(window)) {
        nsIView           *parentWithView;
        nsPoint           origin;
        nsIPluginInstance *inst;
        float             t2p = aPresContext.GetTwipsToPixels();
        nscoord           offx, offy;

        GetOffsetFromView(origin, parentWithView);

        parentWithView->GetScrollOffset(&offx, &offy);

//        window->x = NSTwipsToIntPixels(origin.x, t2p);
//        window->y = NSTwipsToIntPixels(origin.y, t2p);
        window->x = 0;
        window->y = 0;

        window->clipRect.top = 0;
        window->clipRect.left = 0;
//        window->clipRect.top = NSTwipsToIntPixels(origin.y, t2p);
//        window->clipRect.left = NSTwipsToIntPixels(origin.x, t2p);
//        window->clipRect.top = NSTwipsToIntPixels(origin.y + offy, t2p);
//        window->clipRect.left = NSTwipsToIntPixels(origin.x + offx, t2p);
        window->clipRect.bottom = window->clipRect.top + window->height;
        window->clipRect.right = window->clipRect.left + window->width;

        if (NS_OK == mInstanceOwner->GetInstance(inst)) {
          inst->SetWindow(window);
          NS_RELEASE(inst);
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsObjectFrame::Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect)
{
  const nsStyleFont* font =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);

  aRenderingContext.SetFont(font->mFont);
  aRenderingContext.SetColor(NS_RGB(192, 192, 192));
  aRenderingContext.FillRect(0, 0, mRect.width, mRect.height);
  aRenderingContext.SetColor(NS_RGB(0, 0, 0));
  aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  float p2t = aPresContext.GetPixelsToTwips();
  nscoord px3 = NSIntPixelsToTwips(3, p2t);
  nsAutoString tmp;
  nsIAtom* atom;
  mContent->GetTag(atom);
  if (nsnull != atom) {
    atom->ToString(tmp);
    NS_RELEASE(atom);
    aRenderingContext.DrawString(tmp, px3, px3, 0);
  }
  return NS_OK;
}

nsresult
nsObjectFrame::SetURL(const nsString& aURLSpec)
{
  if (nsnull != mURLSpec) {
    delete mURLSpec;
  }
  mURLSpec = new nsString(aURLSpec);
  if (nsnull == mURLSpec) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsObjectFrame::SetFullURL(const nsString& aURLSpec)
{
  if (nsnull != mFullURL) {
    delete mFullURL;
  }
  mFullURL = new nsString(aURLSpec);
  if (nsnull == mFullURL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsObjectFrame::SetBaseHREF(const nsString& aBaseHREF)
{
  if (nsnull != mBaseHREF) {
    delete mBaseHREF;
  }
  mBaseHREF = new nsString(aBaseHREF);
  if (nsnull == mBaseHREF) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsresult
nsObjectFrame::Scrolled(nsIView *aView)
{
  return NS_OK;
}

nsresult nsObjectFrame :: GetFullURL(nsString& aFullURL)
{
  if (nsnull != mFullURL)
    aFullURL = *mFullURL;
  else
    aFullURL = nsString("");

  return NS_OK;
}


nsresult
NS_NewObjectFrame(nsIContent* aContent, nsIFrame* aParentFrame,
                  nsIFrame*& aFrameResult)
{
  aFrameResult = new nsObjectFrame(aContent, aParentFrame);
  if (nsnull == aFrameResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

//plugin instance owner

nsPluginInstanceOwner :: nsPluginInstanceOwner()
{
  NS_INIT_REFCNT();

  memset(&mPluginWindow, 0, sizeof(mPluginWindow));
  mInstance = nsnull;
  mOwner = nsnull;
  mNumAttrs = 0;
  mAttrNames = nsnull;
  mAttrVals = nsnull;
  mWidget = nsnull;
  mContext = nsnull;
}

nsPluginInstanceOwner :: ~nsPluginInstanceOwner()
{
  if (nsnull != mInstance)
  {
    mInstance->Stop();
    mInstance->Destroy();
    NS_RELEASE(mInstance);
  }

  mOwner = nsnull;

  for (PRInt32 cnt = 0; cnt < mNumAttrs; cnt++)
  {
    if ((nsnull != mAttrNames) && (nsnull != mAttrNames[cnt]))
    {
      PR_Free(mAttrNames[cnt]);
      mAttrNames[cnt] = nsnull;
    }

    if ((nsnull != mAttrVals) && (nsnull != mAttrVals[cnt]))
    {
      PR_Free(mAttrVals[cnt]);
      mAttrVals[cnt] = nsnull;
    }
  }

  if (nsnull != mAttrNames)
  {
    PR_Free(mAttrNames);
    mAttrNames = nsnull;
  }

  if (nsnull != mAttrVals)
  {
    PR_Free(mAttrVals);
    mAttrVals = nsnull;
  }

  NS_IF_RELEASE(mWidget);
  mContext = nsnull;
}

static NS_DEFINE_IID(kIPluginInstanceOwnerIID, NS_IPLUGININSTANCEOWNER_IID); 

NS_IMPL_QUERY_INTERFACE(nsPluginInstanceOwner, kIPluginInstanceOwnerIID);
NS_IMPL_ADDREF(nsPluginInstanceOwner);
NS_IMPL_RELEASE(nsPluginInstanceOwner);

NS_IMETHODIMP nsPluginInstanceOwner :: SetInstance(nsIPluginInstance *aInstance)
{
  NS_IF_RELEASE(mInstance);
  mInstance = aInstance;
  NS_IF_ADDREF(mInstance);

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner :: GetWindow(nsPluginWindow *&aWindow)
{
  aWindow = &mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner :: GetMode(nsPluginMode *aMode)
{
printf("instance owner getmode called\n");
  *aMode = nsPluginMode_Embedded;
  return NS_OK;
}

static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);
static NS_DEFINE_IID(kILinkHandlerIID, NS_ILINKHANDLER_IID);

NS_IMETHODIMP nsPluginInstanceOwner :: GetAttributes(PRUint16& n,
                                                     const char*const*& names,
                                                     const char*const*& values)
{
  nsresult          rv;
  nsIHTMLContent    *content;
  nsIContent        *icontent;

  if (nsnull == mAttrNames)
  {
    rv = mOwner->GetContent(icontent);

    if (rv == NS_OK)
    {
      rv = icontent->QueryInterface(kIHTMLContentIID, (void **)&content);

      if (NS_OK == rv)
      {
        nsISupportsArray  *array;

        rv = NS_NewISupportsArray(&array);

        if (NS_OK == rv)
        {
          PRInt32 cnt, numattrs;

          if ((NS_OK == content->GetAllAttributeNames(array, numattrs)) && (numattrs > 0))
          {
            mAttrNames = (char **)PR_Calloc(sizeof(char *) * numattrs, 1);
            mAttrVals = (char **)PR_Calloc(sizeof(char *) * numattrs, 1);

            if ((nsnull != mAttrNames) && (nsnull != mAttrVals))
            {
              for (cnt = 0; cnt < numattrs; cnt++)
              {
                nsIAtom       *atom = (nsIAtom *)array->ElementAt(cnt);
                nsAutoString  name, val;

                if (nsnull != atom)
                {
                  atom->ToString(name);

                  if (NS_CONTENT_ATTR_HAS_VALUE == icontent->GetAttribute(name, val))
                  {
                    mAttrNames[mNumAttrs] = (char *)PR_Malloc(name.Length() + 1);
                    mAttrVals[mNumAttrs] = (char *)PR_Malloc(val.Length() + 1);

                    if ((nsnull != mAttrNames[mNumAttrs]) &&
                        (nsnull != mAttrVals[mNumAttrs]))
                    {
                      name.ToCString(mAttrNames[mNumAttrs], name.Length() + 1);
                      val.ToCString(mAttrVals[mNumAttrs], val.Length() + 1);

                      mNumAttrs++;
                    }
                    else
                    {
                      if (nsnull != mAttrNames[mNumAttrs])
                      {
                        PR_Free(mAttrNames[mNumAttrs]);
                        mAttrNames[mNumAttrs] = nsnull;
                      }

                      if (nsnull != mAttrVals[mNumAttrs])
                      {
                        PR_Free(mAttrVals[mNumAttrs]);
                        mAttrVals[mNumAttrs] = nsnull;
                      }
                    }
                  }

                  NS_RELEASE(atom);
                }
              }
            }
          }

          NS_RELEASE(array);
        }

        NS_RELEASE(content);
      }

      NS_RELEASE(icontent);
    }
  }
  else
    rv = NS_OK;

  n = mNumAttrs;
  names = (const char **)mAttrNames;
  values = (const char **)mAttrVals;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner :: GetAttribute(const char* name, const char* *result)
{
  PRInt32 count;

  if (nsnull == mAttrNames)
  {
    PRUint16  numattrs;
    const char * const *names, * const *vals;

    GetAttributes(numattrs, names, vals);
  }

  for (count = 0; count < mNumAttrs; count++)
  {
    if (0 == PL_strcasecmp(mAttrNames[count], name))
    {
      *result = mAttrVals[count];
      break;
    }
  }

  if (count < mNumAttrs)
    return NS_OK;
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner :: GetInstance(nsIPluginInstance *&aInstance)
{
  if (nsnull != mInstance)
  {
    aInstance = mInstance;
    NS_ADDREF(mInstance);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner :: GetURL(const char *aURL, const char *aTarget, void *aPostData)
{
  nsISupports     *container;
  nsILinkHandler  *lh;
  nsresult        rv;
  nsIView         *view;
  nsPoint         origin;

  if ((nsnull != mOwner) && (nsnull != mContext))
  {
    rv = mOwner->GetOffsetFromView(origin, view);

    if (NS_OK == rv)
    {
      rv = mContext->GetContainer(&container);

      if (NS_OK == rv)
      {
        rv = container->QueryInterface(kILinkHandlerIID, (void **)&lh);

        if (NS_OK == rv)
        {
          nsAutoString  uniurl = nsAutoString(aURL);
          nsAutoString  unitarget = nsAutoString(aTarget);
          nsAutoString  base, fullurl;

          mOwner->GetFullURL(base);

          nsIPresShell  *shell = mContext->GetShell();
          nsIDocument   *doc = shell->GetDocument();
          nsIURL        *docURL = doc->GetDocumentURL();

          // Create an absolute URL
          rv = NS_MakeAbsoluteURL(docURL, base, uniurl, fullurl);

          NS_RELEASE(shell);
          NS_RELEASE(docURL);
          NS_RELEASE(doc);

          if (NS_OK == rv)
            rv = lh->OnLinkClick(mOwner, fullurl.GetUnicode(), unitarget.GetUnicode(), nsnull);

          NS_RELEASE(lh);
        }

        NS_RELEASE(container);
      }
    }
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner :: Init(nsIPresContext* aPresContext, nsObjectFrame *aFrame)
{
  //do not addref to avoid circular refs. MMP
  mContext = aPresContext;
  mOwner = aFrame;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner :: CreateWidget(void)
{
  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;

  if (nsnull != mOwner)
  {
    // Create view if necessary

    mOwner->GetView(view);

    if (nsnull == view)
    {
      PRBool    windowless;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

      rv = mOwner->CreateWidget(mPluginWindow.width,
                                mPluginWindow.height,
                                windowless);
      if (NS_OK == rv)
      {
        nsIView   *view;

        mOwner->GetView(view);
        view->GetWidget(mWidget);

        if (PR_TRUE == windowless)
        {
          mPluginWindow.window = nsnull;    //XXX this needs to be a HDC
          mPluginWindow.type = nsPluginWindowType_Drawable;
        }
        else
        {
          mPluginWindow.window = (nsPluginPort *)mWidget->GetNativeData(NS_NATIVE_WINDOW);
          mPluginWindow.type = nsPluginWindowType_Window;
        }
      }
    }
  }

  return rv;
}
