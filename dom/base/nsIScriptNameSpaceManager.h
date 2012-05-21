/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIScriptNameSpaceManager_h__
#define nsIScriptNameSpaceManager_h__

#define JAVASCRIPT_GLOBAL_CONSTRUCTOR_CATEGORY \
  "JavaScript-global-constructor"

#define JAVASCRIPT_GLOBAL_CONSTRUCTOR_PROTO_ALIAS_CATEGORY \
  "JavaScript-global-constructor-prototype-alias"

#define JAVASCRIPT_GLOBAL_PROPERTY_CATEGORY \
  "JavaScript-global-property"

// a global property that is only accessible to privileged script 
#define JAVASCRIPT_GLOBAL_PRIVILEGED_PROPERTY_CATEGORY \
  "JavaScript-global-privileged-property"

#define JAVASCRIPT_NAVIGATOR_PROPERTY_CATEGORY \
  "JavaScript-navigator-property"

#define JAVASCRIPT_GLOBAL_STATIC_NAMESET_CATEGORY \
  "JavaScript-global-static-nameset"

#define JAVASCRIPT_GLOBAL_DYNAMIC_NAMESET_CATEGORY \
  "JavaScript-global-dynamic-nameset"

#define JAVASCRIPT_DOM_CLASS \
  "JavaScript-DOM-class"

#define JAVASCRIPT_DOM_INTERFACE \
  "JavaScript-DOM-interface"

#endif /* nsIScriptNameSpaceManager_h__ */
