/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://www.whatwg.org/specs/web-apps/current-work/#the-object-element
 * http://www.whatwg.org/specs/web-apps/current-work/#HTMLObjectElement-partial
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

// http://www.whatwg.org/specs/web-apps/current-work/#the-object-element
[NeedResolve,
 Exposed=Window]
interface HTMLObjectElement : HTMLElement {
  [HTMLConstructor] constructor();

  [CEReactions, Pure, SetterThrows]
           attribute DOMString data;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString type;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString name;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString useMap;
  [Pure]
  readonly attribute HTMLFormElement? form;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString width;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString height;
  // Not pure: can trigger about:blank instantiation
  [NeedsSubjectPrincipal]
  readonly attribute Document? contentDocument;
  // Not pure: can trigger about:blank instantiation
  [NeedsSubjectPrincipal]
  readonly attribute WindowProxy? contentWindow;

  readonly attribute boolean willValidate;
  readonly attribute ValidityState validity;
  [Throws]
  readonly attribute DOMString validationMessage;
  boolean checkValidity();
  boolean reportValidity();
  void setCustomValidity(DOMString error);
};

// http://www.whatwg.org/specs/web-apps/current-work/#HTMLObjectElement-partial
partial interface HTMLObjectElement {
  [CEReactions, Pure, SetterThrows]
           attribute DOMString align;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString archive;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString code;
  [CEReactions, Pure, SetterThrows]
           attribute boolean declare;
  [CEReactions, Pure, SetterThrows]
           attribute unsigned long hspace;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString standby;
  [CEReactions, Pure, SetterThrows]
           attribute unsigned long vspace;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString codeBase;
  [CEReactions, Pure, SetterThrows]
           attribute DOMString codeType;

  [CEReactions, Pure, SetterThrows]
           attribute [LegacyNullToEmptyString] DOMString border;
};

partial interface HTMLObjectElement {
  // GetSVGDocument
  [NeedsSubjectPrincipal]
  Document? getSVGDocument();
};

interface mixin MozObjectLoadingContent {
  // Mirrored chrome-only scriptable nsIObjectLoadingContent methods.  Please
  // make sure to update this list if nsIObjectLoadingContent changes.  Also,
  // make sure everything on here is [ChromeOnly].
  [ChromeOnly]
  const unsigned long TYPE_LOADING     = 0;
  [ChromeOnly]
  const unsigned long TYPE_IMAGE       = 1;
  [ChromeOnly]
  const unsigned long TYPE_FALLBACK    = 2;
  [ChromeOnly]
  const unsigned long TYPE_FAKE_PLUGIN = 3;
  [ChromeOnly]
  const unsigned long TYPE_DOCUMENT    = 4;
  [ChromeOnly]
  const unsigned long TYPE_NULL        = 5;

  /**
   * The actual mime type (the one we got back from the network
   * request) for the element.
   */
  [ChromeOnly]
  readonly attribute DOMString actualType;

  /**
   * Gets the type of the content that's currently loaded. See
   * the constants above for the list of possible values.
   */
  [ChromeOnly]
  readonly attribute unsigned long displayedType;

  /**
   * Gets the content type that corresponds to the give MIME type.  See the
   * constants above for the list of possible values.  If nothing else fits,
   * TYPE_NULL will be returned.
   */
  [ChromeOnly]
  unsigned long getContentTypeForMIMEType(DOMString aMimeType);


  [ChromeOnly]
  sequence<MozPluginParameter> getPluginAttributes();

  [ChromeOnly]
  sequence<MozPluginParameter> getPluginParameters();

  /**
   * Forces a re-evaluation and reload of the tag, optionally invalidating its
   * click-to-play state.  This can be used when the MIME type that provides a
   * type has changed, for instance, to force the tag to re-evalulate the
   * handler to use.
   */
  [ChromeOnly, Throws]
  void reload(boolean aClearActivation);

  /**
   * The URL of the data/src loaded in the object. This may be null (i.e.
   * an <embed> with no src).
   */
  [ChromeOnly]
  readonly attribute URI? srcURI;

  /**
   * Disable the use of fake plugins and reload the tag if necessary
   */
  [ChromeOnly, Throws]
  void skipFakePlugins();

  [ChromeOnly, Throws, NeedsCallerType]
  readonly attribute unsigned long runID;
};

/**
 * Name:Value pair type used for passing parameters to NPAPI or javascript
 * plugins.
 */
dictionary MozPluginParameter {
  DOMString name = "";
  DOMString value = "";
};

HTMLObjectElement includes MozImageLoadingContent;
HTMLObjectElement includes MozFrameLoaderOwner;
HTMLObjectElement includes MozObjectLoadingContent;
