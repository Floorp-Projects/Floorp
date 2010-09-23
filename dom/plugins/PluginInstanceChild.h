/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Chris Jones <jones.chris.g@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef dom_plugins_PluginInstanceChild_h
#define dom_plugins_PluginInstanceChild_h 1

#include "mozilla/plugins/PPluginInstanceChild.h"
#include "mozilla/plugins/PluginScriptableObjectChild.h"
#include "mozilla/plugins/StreamNotifyChild.h"
#if defined(OS_WIN)
#include "mozilla/gfx/SharedDIBWin.h"
#elif defined(OS_MACOSX)
#include "nsCoreAnimationSupport.h"
#include "base/timer.h"
#endif

#include "npfunctions.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"
#include "ChildAsyncCall.h"
#include "ChildTimer.h"
#include "nsRect.h"
#include "nsTHashtable.h"
#include "mozilla/PaintTracker.h"
#include "gfxASurface.h"

namespace mozilla {
namespace plugins {

class PBrowserStreamChild;
class BrowserStreamChild;
class StreamNotifyChild;

class PluginInstanceChild : public PPluginInstanceChild
{
    friend class BrowserStreamChild;
    friend class PluginStreamChild;
    friend class StreamNotifyChild; 

#ifdef OS_WIN
    friend LRESULT CALLBACK PluginWindowProc(HWND hWnd,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
#endif

protected:
    virtual bool AnswerNPP_SetWindow(const NPRemoteWindow& window);

    virtual bool
    AnswerNPP_GetValue_NPPVpluginNeedsXEmbed(bool* needs, NPError* rv);
    virtual bool
    AnswerNPP_GetValue_NPPVpluginScriptableNPObject(PPluginScriptableObjectChild** value,
                                                    NPError* result);

    virtual bool
    AnswerNPP_SetValue_NPNVprivateModeBool(const bool& value, NPError* result);

    virtual bool
    AnswerNPP_HandleEvent(const NPRemoteEvent& event, int16_t* handled);
    virtual bool
    AnswerNPP_HandleEvent_Shmem(const NPRemoteEvent& event, Shmem& mem, int16_t* handled, Shmem* rtnmem);
    virtual bool
    AnswerNPP_HandleEvent_IOSurface(const NPRemoteEvent& event, const uint32_t& surface, int16_t* handled);

    // Async rendering
    virtual bool
    RecvAsyncSetWindow(const gfxSurfaceType& aSurfaceType,
                       const NPRemoteWindow& aWindow);
    virtual bool RecvPaintFinished(void);

    NS_OVERRIDE
    virtual bool
    AnswerPaint(const NPRemoteEvent& event, int16_t* handled)
    {
        PaintTracker pt;
        return AnswerNPP_HandleEvent(event, handled);
    }

    NS_OVERRIDE
    virtual bool
    RecvWindowPosChanged(const NPRemoteEvent& event);

    virtual bool
    AnswerNPP_Destroy(NPError* result);

    virtual PPluginScriptableObjectChild*
    AllocPPluginScriptableObject();

    virtual bool
    DeallocPPluginScriptableObject(PPluginScriptableObjectChild* aObject);

    NS_OVERRIDE virtual bool
    RecvPPluginScriptableObjectConstructor(PPluginScriptableObjectChild* aActor);

    virtual PBrowserStreamChild*
    AllocPBrowserStream(const nsCString& url,
                        const uint32_t& length,
                        const uint32_t& lastmodified,
                        PStreamNotifyChild* notifyData,
                        const nsCString& headers,
                        const nsCString& mimeType,
                        const bool& seekable,
                        NPError* rv,
                        uint16_t *stype);

    virtual bool
    AnswerPBrowserStreamConstructor(
            PBrowserStreamChild* aActor,
            const nsCString& url,
            const uint32_t& length,
            const uint32_t& lastmodified,
            PStreamNotifyChild* notifyData,
            const nsCString& headers,
            const nsCString& mimeType,
            const bool& seekable,
            NPError* rv,
            uint16_t* stype);
        
    virtual bool
    DeallocPBrowserStream(PBrowserStreamChild* stream);

    virtual PPluginStreamChild*
    AllocPPluginStream(const nsCString& mimeType,
                       const nsCString& target,
                       NPError* result);

    virtual bool
    DeallocPPluginStream(PPluginStreamChild* stream);

    virtual PStreamNotifyChild*
    AllocPStreamNotify(const nsCString& url, const nsCString& target,
                       const bool& post, const nsCString& buffer,
                       const bool& file,
                       NPError* result);

    NS_OVERRIDE virtual bool
    DeallocPStreamNotify(PStreamNotifyChild* notifyData);

    virtual bool
    AnswerSetPluginFocus();

    virtual bool
    AnswerUpdateWindow();

public:
    PluginInstanceChild(const NPPluginFuncs* aPluginIface, const nsCString& aMimeType);

    virtual ~PluginInstanceChild();

    bool Initialize();

    NPP GetNPP()
    {
        return &mData;
    }

    NPError
    NPN_GetValue(NPNVariable aVariable, void* aValue);

    NPError
    NPN_SetValue(NPPVariable aVariable, void* aValue);

    PluginScriptableObjectChild*
    GetActorForNPObject(NPObject* aObject);

    NPError
    NPN_NewStream(NPMIMEType aMIMEType, const char* aWindow,
                  NPStream** aStream);

    void InvalidateRect(NPRect* aInvalidRect);

    uint32_t ScheduleTimer(uint32_t interval, bool repeat, TimerFunc func);
    void UnscheduleTimer(uint32_t id);

    void AsyncCall(PluginThreadCallback aFunc, void* aUserData);

private:
    friend class PluginModuleChild;

    // Quirks mode support for various plugin mime types
    enum PluginQuirks {
        // Win32: Translate mouse input based on WM_WINDOWPOSCHANGED
        // windowing events due to winless shared dib rendering. See
        // WinlessHandleEvent for details.
        QUIRK_SILVERLIGHT_WINLESS_INPUT_TRANSLATION     = 1 << 0,
        // Win32: Hook TrackPopupMenu api so that we can swap out parent
        // hwnds. The api will fail with parents not associated with our
        // child ui thread. See WinlessHandleEvent for details.
        QUIRK_WINLESS_TRACKPOPUP_HOOK                   = 1 << 1,
        // Win32: Throttle flash WM_USER+1 heart beat messages to prevent
        // flooding chromium's dispatch loop, which can cause ipc traffic
        // processing lag.
        QUIRK_FLASH_THROTTLE_WMUSER_EVENTS              = 1 << 2,
    };

    void InitQuirksModes(const nsCString& aMimeType);

    NPError
    InternalGetNPObjectForValue(NPNVariable aValue,
                                NPObject** aObject);

#if defined(OS_WIN)
    static bool RegisterWindowClass();
    bool CreatePluginWindow();
    void DestroyPluginWindow();
    void ReparentPluginWindow(HWND hWndParent);
    void SizePluginWindow(int width, int height);
    int16_t WinlessHandleEvent(NPEvent& event);
    void CreateWinlessPopupSurrogate();
    void DestroyWinlessPopupSurrogate();
    void InitPopupMenuHook();
    void SetupFlashMsgThrottle();
    void UnhookWinlessFlashThrottle();
    void FlashThrottleMessage(HWND, UINT, WPARAM, LPARAM, bool);
    static LRESULT CALLBACK DummyWindowProc(HWND hWnd,
                                            UINT message,
                                            WPARAM wParam,
                                            LPARAM lParam);
    static LRESULT CALLBACK PluginWindowProc(HWND hWnd,
                                             UINT message,
                                             WPARAM wParam,
                                             LPARAM lParam);
    static BOOL WINAPI TrackPopupHookProc(HMENU hMenu,
                                          UINT uFlags,
                                          int x,
                                          int y,
                                          int nReserved,
                                          HWND hWnd,
                                          CONST RECT *prcRect);
    static BOOL CALLBACK EnumThreadWindowsCallback(HWND hWnd,
                                                   LPARAM aParam);
    static LRESULT CALLBACK WinlessHiddenFlashWndProc(HWND hWnd,
                                                      UINT message,
                                                      WPARAM wParam,
                                                      LPARAM lParam);

    class FlashThrottleAsyncMsg : public ChildAsyncCall
    {
      public:
        FlashThrottleAsyncMsg();
        FlashThrottleAsyncMsg(PluginInstanceChild* aInst, 
                              HWND aWnd, UINT aMsg,
                              WPARAM aWParam, LPARAM aLParam,
                              bool isWindowed)
          : ChildAsyncCall(aInst, nsnull, nsnull),
          mWnd(aWnd),
          mMsg(aMsg),
          mWParam(aWParam),
          mLParam(aLParam),
          mWindowed(isWindowed)
        {}

        NS_OVERRIDE void Run();

        WNDPROC GetProc();
        HWND GetWnd() { return mWnd; }
        UINT GetMsg() { return mMsg; }
        WPARAM GetWParam() { return mWParam; }
        LPARAM GetLParam() { return mLParam; }

      private:
        HWND                 mWnd;
        UINT                 mMsg;
        WPARAM               mWParam;
        LPARAM               mLParam;
        bool                 mWindowed;
    };

#endif

    const NPPluginFuncs* mPluginIface;
    NPP_t mData;
    NPWindow mWindow;
    int mQuirks;

    // Cached scriptable actors to avoid IPC churn
    PluginScriptableObjectChild* mCachedWindowActor;
    PluginScriptableObjectChild* mCachedElementActor;

#if defined(MOZ_X11) && defined(XP_UNIX) && !defined(XP_MACOSX)
    NPSetWindowCallbackStruct mWsInfo;
#elif defined(OS_WIN)
    HWND mPluginWindowHWND;
    WNDPROC mPluginWndProc;
    HWND mPluginParentHWND;
    int mNestedEventLevelDepth;
    HWND mCachedWinlessPluginHWND;
    HWND mWinlessPopupSurrogateHWND;
    nsIntPoint mPluginSize;
    nsIntPoint mPluginOffset;
    WNDPROC mWinlessThrottleOldWndProc;
    HWND mWinlessHiddenMsgHWND;
#endif

    friend class ChildAsyncCall;

    Mutex mAsyncCallMutex;
    nsTArray<ChildAsyncCall*> mPendingAsyncCalls;
    nsTArray<nsAutoPtr<ChildTimer> > mTimers;

    /**
     * During destruction we enumerate all remaining scriptable objects and
     * invalidate/delete them. Enumeration can re-enter, so maintain a
     * hash separate from PluginModuleChild.mObjectMap.
     */
    nsAutoPtr< nsTHashtable<DeletingObjectEntry> > mDeletingHash;

#if defined(OS_WIN)
private:
    // Shared dib rendering management for windowless plugins.
    bool SharedSurfaceSetWindow(const NPRemoteWindow& aWindow);
    int16_t SharedSurfacePaint(NPEvent& evcopy);
    void SharedSurfaceRelease();
    bool AlphaExtractCacheSetup();
    void AlphaExtractCacheRelease();
    void UpdatePaintClipRect(RECT* aRect);

private:
    enum {
      RENDER_NATIVE,
      RENDER_BACK_ONE,
      RENDER_BACK_TWO 
    };
    gfx::SharedDIBWin mSharedSurfaceDib;
    struct {
      PRUint16        doublePass;
      HDC             hdc;
      HBITMAP         bmp;
    } mAlphaExtract;
#endif // defined(OS_WIN)
#if defined(OS_MACOSX)
private:
#if defined(__i386__)
    NPEventModel          mEventModel;
#endif
    CGColorSpaceRef       mShColorSpace;
    CGContextRef          mShContext;
    int16_t               mDrawingModel;
    nsCARenderer          mCARenderer;

public:
    const NPCocoaEvent* getCurrentEvent() {
        return mCurrentEvent;
    }

#if defined(__i386__)
    NPEventModel EventModel() { return mEventModel; }
#endif

private:
    const NPCocoaEvent   *mCurrentEvent;
#endif

    // ShowPluginFrame - in general does four things:
    // 1) Create mCurrentSurface optimized for rendering to parent process
    // 2) Updated mCurrentSurface to be a complete copy of mBackSurface
    // 3) Draw the invalidated plugin area into mCurrentSurface
    // 4) Send it to parent process.
    PRBool ShowPluginFrame(void);

    // Post ShowPluginFrame task
    void AsyncShowPluginFrame(void);

    // In the PaintRect functions, aSurface is the size of the full plugin window. Each PaintRect function
    // renders into the subrectangle aRect of aSurface (possibly more if we're working around a Flash bug).

    // Paint plugin content rectangle to surface with bg color filling
    void PaintRectToSurface(const nsIntRect& aRect,
                            gfxASurface* aSurface,
                            const gfxRGBA& aColor);

    // Render plugin content to surface using
    // white/black image alpha extraction algorithm
    void PaintRectWithAlphaExtraction(const nsIntRect& aRect,
                                      gfxASurface* aSurface);

    // Call plugin NPAPI function to render plugin content to surface
    // @param - aSurface - should be compatible with current platform plugin rendering
    // @return - FALSE if plugin not painted to surface
    void PaintRectToPlatformSurface(const nsIntRect& aRect,
                                    gfxASurface* aSurface);

    // Update NPWindow platform attributes and call plugin "setwindow"
    // @param - aForceSetWindow - call setwindow even if platform attributes are the same
    void UpdateWindowAttributes(PRBool aForceSetWindow = PR_FALSE);

    // Create optimized mCurrentSurface for parent process rendering
    // @return FALSE if optimized surface not created
    PRBool CreateOptSurface(void);

    // Create mHelperSurface if mCurrentSurface non compatible with plugins
    // @return TRUE if helper surface created successfully, or not needed
    PRBool MaybeCreatePlatformHelperSurface(void);

    // Make sure that we have surface for rendering
    PRBool EnsureCurrentBuffer(void);

    // Helper function for delayed InvalidateRect call
    // non null mCurrentInvalidateTask will call this function
    void InvalidateRectDelayed(void);

    // Set as true when SetupLayer called
    // and go with different path in InvalidateRect function
    PRPackedBool          mLayersRendering;
    // Current surface available for rendering
    nsRefPtr<gfxASurface> mCurrentSurface;
    // Back surface, just keeping reference to
    // surface which is on ParentProcess side
    nsRefPtr<gfxASurface> mBackSurface;
    // Accumulated invalidate rect, while back buffer is not accessible
    nsIntRect             mAccumulatedInvalidRect;
    // Plugin only call SetTransparent
    // and does not remember their transparent state
    // and p->getvalue return always false
    PRPackedBool          mIsTransparent;
    // Surface type optimized of parent process
    gfxSurfaceType        mSurfaceType;

    // set TRUE if plugin surface dropped in asyncSetWindow
    // if TRUE then initiate full repaint in RecvPaintFinished
    PRPackedBool          mPendingForcePaint;

    // Keep InvalidateRect task pointer to be able Cancel it on Destroy
    CancelableTask       *mCurrentInvalidateTask;

    // True while plugin-child in plugin call
    // Use to prevent plugin paint re-enter
    PRPackedBool          mPendingPluginCall;

    // On some platforms, plugins may not support rendering to a surface with
    // alpha, or not support rendering to an image surface.
    // In those cases we need to draw to a temporary platform surface; we cache
    // that surface here.
    nsRefPtr<gfxASurface> mHelperSurface;

    // true when plugin does not support painting to ARGB32 surface
    // this is false for maemo platform, and false if plugin
    // supports NPPVpluginTransparentAlphaBool (which is not part of NPAPI yet)
    PRPackedBool          mDoAlphaExtraction;

    // Cached rectangle rendered to previous surface(mBackSurface)
    // Used for reading back to current surface and syncing data
    nsIntRect             mSurfaceDifferenceRect;

#ifdef MOZ_X11
    // Used with windowless flash plugin only, see bug 574583
    PRPackedBool          mFlash10Quirks;
#endif
};

} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginInstanceChild_h
