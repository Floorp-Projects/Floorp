/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
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

#ifndef nsXBLSerialize_h__
#define nsXBLSerialize_h__

#include "jsapi.h"

#include "nsIScriptContext.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsINameSpaceManager.h"

typedef PRUint8 XBLBindingSerializeDetails;

// A version number to ensure we don't load cached data in a different
// file format.
#define XBLBinding_Serialize_Version 0x00000001

// Set for the first binding in a document
#define XBLBinding_Serialize_IsFirstBinding 1

// Set to indicate that nsXBLPrototypeBinding::mInheritStyle should be true
#define XBLBinding_Serialize_InheritStyle 2

// Appears at the end of the serialized data to indicate that no more bindings
// are present for this document.
#define XBLBinding_Serialize_NoMoreBindings 0x80

// Implementation member types. The serialized value for each member contains one
// of these values, combined with the read-only flag XBLBinding_Serialize_ReadOnly.
// Use XBLBinding_Serialize_Mask to filter out the read-only flag and check for
// just the member type.
#define XBLBinding_Serialize_NoMoreItems 0 // appears at the end of the members list
#define XBLBinding_Serialize_Field 1
#define XBLBinding_Serialize_GetterProperty 2
#define XBLBinding_Serialize_SetterProperty 3
#define XBLBinding_Serialize_GetterSetterProperty 4
#define XBLBinding_Serialize_Method 5
#define XBLBinding_Serialize_Constructor 6
#define XBLBinding_Serialize_Destructor 7
#define XBLBinding_Serialize_Handler 8
#define XBLBinding_Serialize_Image 9
#define XBLBinding_Serialize_Stylesheet 10
#define XBLBinding_Serialize_Mask 0x0F
#define XBLBinding_Serialize_ReadOnly 0x80

// Appears at the end of the list of insertion points to indicate that there
// are no more. 
#define XBLBinding_Serialize_NoMoreInsertionPoints 0xFFFFFFFF

// When serializing content nodes, a single-byte namespace id is written out
// first. The special values below can appear in place of a namespace id.

// Indicates that this is not one of the built-in namespaces defined in 
// nsINameSpaceManager.h. The string form will be serialized immediately
// following.
#define XBLBinding_Serialize_CustomNamespace 0xFE

// Flags to indicate a non-element node. Otherwise, it is an element. 
#define XBLBinding_Serialize_TextNode 0xFB
#define XBLBinding_Serialize_CDATANode 0xFC
#define XBLBinding_Serialize_CommentNode 0xFD

// Indicates that there is no content to serialize/deserialize
#define XBLBinding_Serialize_NoContent 0xFF

// Appears at the end of the forwarded attributes list to indicate that there
// are no more attributes.
#define XBLBinding_Serialize_NoMoreAttributes 0xFF

PR_STATIC_ASSERT(XBLBinding_Serialize_CustomNamespace >= kNameSpaceID_LastBuiltin);

nsresult
XBL_SerializeFunction(nsIScriptContext* aContext,
                      nsIObjectOutputStream* aStream,
                      JSObject* aFunctionObject);

nsresult
XBL_DeserializeFunction(nsIScriptContext* aContext,
                        nsIObjectInputStream* aStream,
                        JSObject** aFunctionObject);

#endif // nsXBLSerialize_h__
