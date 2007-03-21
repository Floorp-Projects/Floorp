# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Alec Flett      <alecf@netscape.com>
#   Ben Goodger     <ben@netscape.com>
#   Mike Pinkerton  <pinkerton@netscape.com>
#   Blake Ross      <blakeross@telocity.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

/*
 * - [ Dependencies ] ---------------------------------------------------------
 *  utilityOverlay.js:
 *    - gatherTextUnder
 */

  var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  
  /** 
   * extract the href from the link click event. 
   * We look for HTMLAnchorElement, HTMLAreaElement, HTMLLinkElement,
   * HTMLInputElement.form.action, and nested anchor tags.
   * 
   * @return href for the url being clicked
   */
  function hRefForClickEvent(event)
  {
    var target = event.target;
    var href;
    var isKeyPress = (event.type == "keypress");

    if (target instanceof HTMLAnchorElement ||
        target instanceof HTMLAreaElement   ||
        target instanceof HTMLLinkElement)
    {
      if (target.hasAttribute("href")) 
        href = target.href;
    }
    else if (target instanceof HTMLInputElement)
    {
      if (target.form && target.form.action)
        href = target.form.action;      
    }
    else 
    {
      // we may be nested inside of a link node
      var linkNode = event.originalTarget;
      while (linkNode && !(linkNode instanceof HTMLAnchorElement))
        linkNode = linkNode.parentNode;
      
      if (linkNode)
        href = linkNode.href;
    }

    return href;
  }

  // Called whenever the user clicks in the content area,
  // except when left-clicking on links (special case)
  // should always return true for click to go through
  function contentAreaClick(event) 
  {
    var href = hRefForClickEvent(event);
    if (href) 
    {
      handleLinkClick(event, href, null);
      if (!event.button)  // left click only
        return gPhishingDetector.warnOnSuspiciousLinkClick(href); // let the phishing detector check the link
    }
    else if (!event.button)
    {
      var targ = event.target;
      // is this an image that we might want to scale?
      const Ci = Components.interfaces;
      if (targ instanceof Ci.nsIImageLoadingContent) 
      {
        // make sure it loaded successfully
        var req = targ.getRequest(Ci.nsIImageLoadingContent.CURRENT_REQUEST);
        if (!req || req.imageStatus & Ci.imgIRequest.STATUS_ERROR)
          return true;
        // is it an inline attachment?
        if (targ.className == "moz-attached-image-scaled")
          targ.className = "moz-attached-image-unscaled";
        else if (targ.className == "moz-attached-image-unscaled")
          targ.className = "moz-attached-image-scaled";
      }
    }
    
    return true;
  }

  function openNewTabOrWindow(event, href, sendReferrer)
  {
    // always return false for stand alone mail (MOZ_THUNDERBIRD)
    // let someone else deal with it
    return false;
  }

  function getContentFrameURI(aFocusedWindow)
  {
    var contentFrame = isContentFrame(aFocusedWindow) ? aFocusedWindow : window.content;
    return contentFrame.location.href;
  }

  function handleLinkClick(event, href, linkNode)
  {
    // Make sure we are allowed to open this URL
    var focusedWindow = document.commandDispatcher.focusedWindow;
    var sourceURL = getContentFrameURI(focusedWindow);
    urlSecurityCheck(href, sourceURL);
    return false;
  }

  function middleMousePaste( event )
  {
    return false;
  }

  function makeURLAbsolute( base, url ) 
  {
    // Construct nsIURL.
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                  .getService(Components.interfaces.nsIIOService);
    var baseURI  = ioService.newURI(base, null, null);

    return ioService.newURI(baseURI.resolve(url), null, null).spec;
  }
