/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Axel Hecht.
 * Portions created by Axel Hecht are Copyright (C) 2001 Axel Hecht.
 * All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org> (Original Author)
 */

// ----------------------
// DiffDOM(node1,node2)
// ----------------------

function DiffDOM(node1,node2)
{
  return DiffNodeAndChildren(node1, node2);
}


// This function does the work of DiffDOM by recursively calling
// itself to explore the tree
function DiffNodeAndChildren(node1, node2)
{
  if (!node1 && !node2) return true;
  if (!node1 || !node2) return false;
  if (node1.type!=node2.type){
    return false;
  }
  var attributes = node1.attributes;
  if ( attributes && attributes.length ) {
    if (!node2.attributes || node2.attributes.length!=attributes.length)
      return false;
    var item, name, value;
    
    for ( var index = 0; index < attributes.length; index++ ) {
      item = attributes.item(index);
      name = item.nodeName;
      value = item.nodeValue;
      if (node2.getAttribute(name)!=value){
        return false;
      }
    }
  }
  
  if (node1.nodeName!=node2.nodeName) return false;
  if (node1.nodeValue!=node2.nodeValue) return false;
  if (node1.hasChildNodes() != node2.hasChildNodes()) return false;
  if ( node1.childNodes ) {
   if (node1.childNodes.length != node2.childNodes.length) return false;
   for ( var child = 0; child < node1.childNodes.length; child++ )
      if (!DiffNodeAndChildren(node1.childNodes[child], node2.childNodes[child])) {
        return false;
      }
  }
  return true;
}
