// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

/**
 * dumb model class to provide default values for sites.
 * link parameter/model object expected to have a .url property, and optionally .title
 */
function Site(aLink) {
  if (!aLink.url) {
    throw Cr.NS_ERROR_INVALID_ARG;
  }
  this._link = aLink;
}

Site.prototype = {
  icon: '',
  get url() {
    return this._link.url;
  },
  get title() {
    // use url if no title was recorded
    return this._link.title || this._link.url;
  },
  get label() {
    // alias for .title
    return this.title;
  },
  get pinned() {
    return NewTabUtils.pinnedLinks.isPinned(this);
  },
  get contextActions() {
    return [
      'delete', // delete means hide here
      this.pinned ? 'unpin' : 'pin'
    ];
  },
  get blocked() {
    return NewTabUtils.blockedLinks.isBlocked(this);
  },
  get attributeValues() {
    return {
      value: this.url,
      label: this.title,
      pinned: this.pinned ? true : undefined,
      selected: this.selected,
      customColor: this.color,
      customImage: this.backgroundImage,
      iconURI: this.icon,
      "data-contextactions": this.contextActions.join(',')
    };
  },
  applyToTileNode: function(aNode) {
    // apply this site's properties as attributes on a tile element
    // the decorated node acts as a view-model for the tile binding
    let attrs = this.attributeValues;
    for (let key in attrs) {
      if (undefined === attrs[key]) {
        aNode.removeAttribute(key);
      } else {
        aNode.setAttribute(key, attrs[key]);
      }
    }
    // is binding already applied?
    if ('refresh' in aNode) {
      // just update it
      aNode.refresh();
    } else {
      // these attribute values will get picked up later when the binding is applied
    }
  }
};
