/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

interface nsIDocShell;
interface nsISupports;
interface nsISessionStoreRestoreData;

/**
 * A callback passed to SessionStoreUtils.forEachNonDynamicChildFrame().
 */
callback SessionStoreUtilsFrameCallback = void (WindowProxy frame, unsigned long index);

/**
 * SessionStore utility functions implemented in C++ for performance reasons.
 */
[ChromeOnly, Exposed=Window]
namespace SessionStoreUtils {
  /**
   * Calls the given |callback| once for each non-dynamic child frame of the
   * given |window|.
   */
  [Throws]
  void forEachNonDynamicChildFrame(WindowProxy window,
                                   SessionStoreUtilsFrameCallback callback);

  /**
   * Takes the given listener, wraps it in a filter that filters out events from
   * dynamic docShells, and adds that filter as a listener for the given event
   * type on the given event target.  The listener that was added is returned
   * (as nsISupports) so that it can later be removed via
   * removeDynamicFrameFilteredListener.
   *
   * This is implemented as a native filter, rather than a JS-based one, for
   * performance reasons.
   */
  [Throws]
  nsISupports? addDynamicFrameFilteredListener(EventTarget target,
                                               DOMString type,
                                               any listener,
                                               boolean useCapture,
                                               optional boolean mozSystemGroup = false);

  /**
   * Remove the passed-in filtered listener from the given event target, if it's
   * currently a listener for the given event type there.  The 'listener'
   * argument must be something that was returned by
   * addDynamicFrameFilteredListener.
   *
   * This is needed, instead of the normal removeEventListener, because the
   * caller doesn't actually have something that WebIDL considers an
   * EventListener.
   */
  [Throws]
  void removeDynamicFrameFilteredListener(EventTarget target,
                                          DOMString type,
                                          nsISupports listener,
                                          boolean useCapture,
                                          optional boolean mozSystemGroup = false);

  /*
   * Save the docShell.allow* properties
   */
  ByteString collectDocShellCapabilities(nsIDocShell docShell);

  /*
   * Restore the docShell.allow* properties
   */
  void restoreDocShellCapabilities(nsIDocShell docShell,
                                   ByteString disallowCapabilities);

  /**
   * Collects scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param document (DOMDocument)
   *
   * @return {scroll: "x,y"} e.g. {scroll: "100,200"}
   *         Returns null when there is no scroll data we want to store for the
   *         given |frame|.
   */
  CollectedData? collectScrollPosition(WindowProxy window);

  /**
   * Restores scroll position data for any given |frame| in the frame hierarchy.
   *
   * @param frame (DOMWindow)
   * @param value (object, see collectScrollPosition())
   */
  void restoreScrollPosition(Window frame, optional CollectedData data = {});

  /**
   * Collect form data for a given |frame| *not* including any subframes.
   *
   * The returned object may have an "id", "xpath", or "innerHTML" key or a
   * combination of those three. Form data stored under "id" is for input
   * fields with id attributes. Data stored under "xpath" is used for input
   * fields that don't have a unique id and need to be queried using XPath.
   * The "innerHTML" key is used for editable documents (designMode=on).
   *
   * Example:
   *   {
   *     id: {input1: "value1", input3: "value3"},
   *     xpath: {
   *       "/xhtml:html/xhtml:body/xhtml:input[@name='input2']" : "value2",
   *       "/xhtml:html/xhtml:body/xhtml:input[@name='input4']" : "value4"
   *     }
   *   }
   *
   * @return object
   *         Returns null when there is no scroll data
   */
  CollectedData? collectFormData(WindowProxy window);

  boolean restoreFormData(Document document, optional CollectedData data = {});

  /**
   * Restores all sessionStorage "super cookies".
   * @param aDocShell
   *        A tab's docshell (containing the sessionStorage)
   * @param aStorageData
   *        A nested object with storage data to be restored that has hosts as
   *        keys and per-origin session storage data as strings. For example:
   *        {"https://example.com^userContextId=1": {"key": "value", "my_number": "123"}}
   */
   void restoreSessionStorage(nsIDocShell docShell, record<DOMString, record<DOMString, DOMString>> data);

   nsISessionStoreRestoreData constructSessionStoreRestoreData();

   [Throws]
   Promise<void> initializeRestore(CanonicalBrowsingContext browsingContext,
                                   nsISessionStoreRestoreData? data);
};

[GenerateConversionToJS, GenerateInit]
dictionary CollectedFileListValue
{
  DOMString type = "file";
  required sequence<DOMString> fileList;
};

[GenerateConversionToJS, GenerateInit]
dictionary CollectedNonMultipleSelectValue
{
  required long selectedIndex;
  required DOMString value;
};

// object contains either a CollectedFileListValue or a CollectedNonMultipleSelectValue or Sequence<DOMString>
typedef (DOMString or boolean or object) CollectedFormDataValue;

dictionary CollectedData
{
  ByteString scroll;
  record<DOMString, CollectedFormDataValue> id;
  record<DOMString, CollectedFormDataValue> xpath;
  DOMString innerHTML;
  ByteString url;
  // mChildren contains CollectedData instances
  sequence<object?> children;
};

dictionary InputElementData {
  sequence<DOMString> id;
  sequence<DOMString> type;
  sequence<long> valueIdx;
  sequence<long> selectedIndex;
  sequence<DOMString> selectVal;
  sequence<DOMString> strVal;
  sequence<boolean> boolVal;
};

[GenerateConversionToJS]
dictionary UpdateSessionStoreData {
  ByteString docShellCaps;
  boolean isPrivate;
  // for sessionStorage
  sequence<ByteString> storageOrigins;
  sequence<DOMString> storageKeys;
  sequence<DOMString> storageValues;
  boolean isFullStorage;
};

[GenerateConversionToJS]
dictionary SessionStoreWindowStateChange {
  SessionStoreFormData formdata;
  SessionStoreScroll scroll;
  boolean hasChildren;
  required sequence<unsigned long> path;
};

dictionary SessionStoreFormData {
  ByteString url;
  record<DOMString, CollectedFormDataValue> id;
  record<DOMString, CollectedFormDataValue> xpath;
  DOMString innerHTML;
};

dictionary SessionStoreScroll {
  ByteString scroll;
};
