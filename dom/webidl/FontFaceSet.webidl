/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://dev.w3.org/csswg/css-font-loading/#FontFaceSet-interface
 *
 * Copyright © 2014 W3C® (MIT, ERCIM, Keio, Beihang), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

enum FontFaceSetLoadStatus { "loading", "loaded" };

// Bug 1072762 is for the FontFaceSet constructor.
// [Constructor(sequence<FontFace> initialFaces)]
[Pref="layout.css.font-loading-api.enabled"]
interface FontFaceSet : EventTarget {

  // Emulate the Set interface, until we can extend Set correctly.
  // Implementing these commented out operations and the iterator is
  // bug 1072101.
  // readonly attribute unsigned long size;
  [Throws] void add(FontFace font);
  boolean has(FontFace font);
  [Throws] boolean delete(FontFace font);
  void clear();
  // Iterator entries();
  // Iterator keys();
  // Iterator values();
  // void forEach(ForEachCallback cb, optional any thisArg);
  // FontFace iterator;

  // -- events for when loading state changes
  attribute EventHandler onloading;
  attribute EventHandler onloadingdone;
  attribute EventHandler onloadingerror;

  // check and start loads if appropriate
  // and fulfill promise when all loads complete
  // Not implemented yet: bug 1072102.
  [Throws] Promise<sequence<FontFace>> load(DOMString font, optional DOMString text = " ");

  // return whether all fonts in the fontlist are loaded
  // (does not initiate load if not available)
  // Not implemented yet: bug 1072102.
  // [Throws] boolean check(DOMString font, optional DOMString text = " ");

  // async notification that font loading and layout operations are done
  [Throws] readonly attribute Promise<void> ready;

  // loading state, "loading" while one or more fonts loading, "loaded" otherwise
  readonly attribute FontFaceSetLoadStatus status;
};

// This provides access to the FontFace objects in the FontFaceSet until we
// get iterators working (bug 1072101).  Don't enable the pref for the CSS Font
// Loading API until the iterator is available, as we don't want to expose more
// indexed properties on the Web.
partial interface FontFaceSet {
  getter FontFace (unsigned long index);
  readonly attribute unsigned long length;
};
