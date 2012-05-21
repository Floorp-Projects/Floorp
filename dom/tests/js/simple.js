/* -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var node = document.documentElement

node.nodeType

node.tagName

var attrList = node.attributes

attrList.length

var attr = attrList.item(0)

attr.name

attr.value

node.hasChildNodes

var children = node.childNodes

children.length

node = children.item(1);
node.nodeType

node.tagName

node = node.firstChild

node = node.nextSibling

node = node.parentNode

 