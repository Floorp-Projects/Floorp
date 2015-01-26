/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_relationtype_h_
#define mozilla_a11y_relationtype_h_

namespace mozilla {
namespace a11y {

enum class RelationType {

  /**
   * This object is labelled by a target object.
   */
  LABELLED_BY = 0x00,

  /**
   * This object is label for a target object.
   */
  LABEL_FOR = 0x01,

  /**
   * This object is described by the target object.
   */
  DESCRIBED_BY = 0x02,

  /**
   * This object is describes the target object.
   */
  DESCRIPTION_FOR = 0x3,

  /**
   * This object is a child of a target object.
   */
  NODE_CHILD_OF = 0x4,

  /**
   * This object is a parent of a target object. A dual relation to
   * NODE_CHILD_OF.
   */
  NODE_PARENT_OF = 0x5,

  /**
   * Some attribute of this object is affected by a target object.
   */
  CONTROLLED_BY = 0x06,

  /**
   * This object is interactive and controls some attribute of a target object.
   */
  CONTROLLER_FOR = 0x07,

  /**
   * Content flows from this object to a target object, i.e. has content that
   * flows logically to another object in a sequential way, e.g. text flow.
   */
  FLOWS_TO = 0x08,

  /**
   * Content flows to this object from a target object, i.e. has content that
   * flows logically from another object in a sequential way, e.g. text flow.
   */
  FLOWS_FROM = 0x09,

  /**
   * This object is a member of a group of one or more objects. When there is
   * more than one object in the group each member may have one and the same
   * target, e.g. a grouping object.  It is also possible that each member has
   * multiple additional targets, e.g. one for every other member in the group.
   */
  MEMBER_OF = 0x0a,

  /**
   * This object is a sub window of a target object.
   */
  SUBWINDOW_OF = 0x0b,

  /**
   * This object embeds a target object. This relation can be used on the
   * OBJID_CLIENT accessible for a top level window to show where the content
   * areas are.
   */
  EMBEDS = 0x0c,

  /**
   * This object is embedded by a target object.
   */
  EMBEDDED_BY = 0x0d,

  /**
   * This object is a transient component related to the target object. When
   * this object is activated the target object doesn't lose focus.
   */
  POPUP_FOR = 0x0e,

  /**
   * This object is a parent window of the target object.
   */
  PARENT_WINDOW_OF = 0x0f,

  /**
   * Part of a form/dialog with a related default button. It is used for
   * MSAA/XPCOM, it isn't for IA2 or ATK.
   */
  DEFAULT_BUTTON = 0x10,

  /**
   * The target object is the containing document object.
   */
  CONTAINING_DOCUMENT = 0x11,

  /**
   * The target object is the topmost containing document object in the tab pane.
   */
  CONTAINING_TAB_PANE = 0x12,

  /**
   * The target object is the containing window object.
   */
  CONTAINING_WINDOW = 0x13,

  /**
   * The target object is the containing application object.
   */
  CONTAINING_APPLICATION = 0x14,

  LAST = CONTAINING_APPLICATION

};

} // namespace a11y
} // namespace mozilla

#endif
