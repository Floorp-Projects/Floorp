/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const BUTTONID = "test-XUL-wrapper-widget";
add_task(function() {
  let btn = createDummyXULButton(BUTTONID, "XUL btn");
  gNavToolbox.palette.appendChild(btn);
  let groupWrapper = CustomizableUI.getWidget(BUTTONID);
  ok(groupWrapper, "Should get a group wrapper");
  let singleWrapper = groupWrapper.forWindow(window);
  ok(singleWrapper, "Should get a single wrapper");
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  CustomizableUI.addWidgetToArea(BUTTONID, CustomizableUI.AREA_NAVBAR);

  let otherSingleWrapper = groupWrapper.forWindow(window);
  is(singleWrapper, otherSingleWrapper, "Should get the same wrapper after adding the node to the navbar.");
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  CustomizableUI.removeWidgetFromArea(BUTTONID);

  otherSingleWrapper = groupWrapper.forWindow(window);
  isnot(singleWrapper, otherSingleWrapper, "Shouldn't get the same wrapper after removing it from the navbar.");
  singleWrapper = otherSingleWrapper;
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  btn.remove();
  otherSingleWrapper = groupWrapper.forWindow(window);
  is(singleWrapper, otherSingleWrapper, "Should get the same wrapper after physically removing the node.");
  is(singleWrapper.node, null, "Wrapper's node should be null now that it's left the DOM.");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, null, "That instance should be null.");

  btn = createDummyXULButton(BUTTONID, "XUL btn");
  gNavToolbox.palette.appendChild(btn);
  otherSingleWrapper = groupWrapper.forWindow(window);
  is(singleWrapper, otherSingleWrapper, "Should get the same wrapper after readding the node.");
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  CustomizableUI.addWidgetToArea(BUTTONID, CustomizableUI.AREA_NAVBAR);

  otherSingleWrapper = groupWrapper.forWindow(window);
  is(singleWrapper, otherSingleWrapper, "Should get the same wrapper after adding the node to the navbar.");
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  CustomizableUI.removeWidgetFromArea(BUTTONID);

  otherSingleWrapper = groupWrapper.forWindow(window);
  isnot(singleWrapper, otherSingleWrapper, "Shouldn't get the same wrapper after removing it from the navbar.");
  singleWrapper = otherSingleWrapper;
  is(singleWrapper.node, btn, "Node should be in the wrapper");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, btn, "Button should be that instance.");

  btn.remove();
  otherSingleWrapper = groupWrapper.forWindow(window);
  is(singleWrapper, otherSingleWrapper, "Should get the same wrapper after physically removing the node.");
  is(singleWrapper.node, null, "Wrapper's node should be null now that it's left the DOM.");
  is(groupWrapper.instances.length, 1, "There should be 1 instance on the group wrapper");
  is(groupWrapper.instances[0].node, null, "That instance should be null.");
});
