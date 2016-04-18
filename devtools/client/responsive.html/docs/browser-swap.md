# Overview

The RDM tool uses several forms of tab and browser swapping to integrate the
tool UI cleanly into the browser UI.  The high level steps of this process are
documented at `/devtools/docs/responsive-design-mode.md`.

This document contains a random assortment of low level notes about the steps
the browser goes through when swapping browsers between tabs.

# Connections between Browsers and Tabs

Link between tab and browser (`gBrowser._linkBrowserToTab`):

```
aTab.linkedBrowser = browser;
gBrowser._tabForBrowser.set(browser, aTab);
```

# Swapping Browsers between Tabs

## Legend

* (R): remote browsers only
* (!R): non-remote browsers only

## Functions Called

When you call `gBrowser.swapBrowsersAndCloseOther` to move tab content from a
browser in one tab to a browser in another tab, here are all the code paths
involved:

* `gBrowser.swapBrowsersAndCloseOther`
  * `gBrowser._beginRemoveTab`
    * `gBrowser.tabContainer.updateVisibility`
    * Emit `TabClose`
    * `browser.webProgress.removeProgressListener`
    * `filter.removeProgressListener`
    * `listener.destroy`
  * `gBrowser._swapBrowserDocShells`
    * `ourBrowser.webProgress.removeProgressListener`
    * `filter.removeProgressListener`
    * `gBrowser._swapRegisteredOpenURIs`
    * `ourBrowser.swapDocShells(aOtherBrowser)`
      * Emit `SwapDocShells`
      * `PopupNotifications._swapBrowserNotifications`
      * `browser.detachFormFill` (!R)
      * `browser.swapFrameLoaders`
      * `browser.attachFormFill` (!R)
      * `browser._remoteWebNavigationImpl.swapBrowser(browser)` (R)
      * `browser._remoteWebProgressManager.swapBrowser(browser)` (R)
      * `browser._remoteFinder.swapBrowser(browser)` (R)
    * `gBrowser.mTabProgressListener`
    * `filter.addProgressListener`
    * `ourBrowser.webProgress.addProgressListener`
  * `gBrowser._endRemoveTab`
    * `gBrowser.tabContainer._fillTrailingGap`
    * `gBrowser._blurTab`
    * `gBrowser._tabFilters.delete`
    * `gBrowser._tabListeners.delete`
    * `gBrowser._outerWindowIDBrowserMap.delete`
    * `browser.destroy`
    * `gBrowser.tabContainer.removeChild`
    * `gBrowser.tabContainer.adjustTabstrip`
    * `gBrowser.tabContainer._setPositionalAttributes`
    * `browser.parentNode.removeChild(browser)`
    * `gBrowser._tabForBrowser.delete`
    * `gBrowser.mPanelContainer.removeChild`
  * `gBrowser.setTabTitle` / `gBrowser.setTabTitleLoading`
    * `browser.currentURI.spec`
    * `gBrowser._tabAttrModified`
    * `gBrowser.updateTitlebar`
  * `gBrowser.updateCurrentBrowser`
    * `browser.docShellIsActive` (!R)
    * `gBrowser.showTab`
    * `gBrowser._appendStatusPanel`
    * `gBrowser._callProgressListeners` with `onLocationChange`
    * `gBrowser._callProgressListeners` with `onSecurityChange`
    * `gBrowser._callProgressListeners` with `onUpdateCurrentBrowser`
    * `gBrowser._recordTabAccess`
    * `gBrowser.updateTitlebar`
    * `gBrowser._callProgressListeners` with `onStateChange`
    * `gBrowser._setCloseKeyState`
    * Emit `TabSelect`
    * `gBrowser._tabAttrModified`
    * `browser.getInPermitUnload`
    * `gBrowser.tabContainer._setPositionalAttributes`
  * `gBrowser._tabAttrModified`

## Browser State

When calling `gBrowser.swapBrowsersAndCloseOther`, the browser is not actually
moved from one tab to the other.  Instead, various properties _on_ each of the
browsers are swapped.

Browser attributes `gBrowser.swapBrowsersAndCloseOther` transfers between
browsers:

* `usercontextid`

Tab attributes `gBrowser.swapBrowsersAndCloseOther` transfers between tabs:

* `usercontextid`
* `muted`
* `soundplaying`
* `busy`

Browser properties `gBrowser.swapBrowsersAndCloseOther` transfers between
browsers:

* `mIconURL`
* `getFindBar(aOurTab)._findField.value`

Browser properties `gBrowser._swapBrowserDocShells` transfers between browsers:

* `outerWindowID` in `gBrowser._outerWindowIDBrowserMap`
* `_outerWindowID` on the browser (R)
* `docShellIsActive`
* `permanentKey`
* `registeredOpenURI`

Browser properties `browser.swapDocShells` transfers between browsers:

* `_docShell`
* `_webBrowserFind`
* `_contentWindow`
* `_webNavigation`
* `_remoteWebNavigation` (R)
* `_remoteWebNavigationImpl` (R)
* `_remoteWebProgressManager` (R)
* `_remoteWebProgress` (R)
* `_remoteFinder` (R)
* `_securityUI` (R)
* `_documentURI` (R)
* `_documentContentType` (R)
* `_contentTitle` (R)
* `_characterSet` (R)
* `_contentPrincipal` (R)
* `_imageDocument` (R)
* `_fullZoom` (R)
* `_textZoom` (R)
* `_isSyntheticDocument` (R)
* `_innerWindowID` (R)
* `_manifestURI` (R)

`browser.swapFrameLoaders` swaps the actual page content.
