/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useRef, useCallback } from "react";
import { Localized } from "./MSLocalized";

export const SubmenuButton = props => {
  return document.createXULElement ? <SubmenuButtonInner {...props} /> : null;
};

function translateMenuitem(item, element) {
  let { label } = item;
  if (!label) {
    return;
  }
  if (label.raw) {
    element.setAttribute("label", label.raw);
  }
  if (label.access_key) {
    element.setAttribute("accesskey", label.access_key);
  }
  if (label.aria_label) {
    element.setAttribute("aria-label", label.aria_label);
  }
  if (label.tooltip_text) {
    element.setAttribute("tooltiptext", label.tooltip_text);
  }
  if (label.string_id) {
    element.setAttribute("data-l10n-id", label.string_id);
    if (label.args) {
      element.setAttribute("data-l10n-args", JSON.stringify(label.args));
    }
  }
}

function addMenuitems(items, popup) {
  for (let item of items) {
    switch (item.type) {
      case "separator":
        popup.appendChild(document.createXULElement("menuseparator"));
        break;
      case "menu":
        let menu = document.createXULElement("menu");
        menu.className = "fxms-multi-stage-menu";
        translateMenuitem(item, menu);
        if (item.id) {
          menu.value = item.id;
        }
        if (item.icon) {
          menu.classList.add("menu-iconic");
          menu.setAttribute("image", item.icon);
        }
        popup.appendChild(menu);
        let submenuPopup = document.createXULElement("menupopup");
        menu.appendChild(submenuPopup);
        addMenuitems(item.submenu, submenuPopup);
        break;
      case "action":
        let menuitem = document.createXULElement("menuitem");
        translateMenuitem(item, menuitem);
        menuitem.config = item;
        if (item.id) {
          menuitem.value = item.id;
        }
        if (item.icon) {
          menuitem.classList.add("menuitem-iconic");
          menuitem.setAttribute("image", item.icon);
        }
        popup.appendChild(menuitem);
        break;
    }
  }
}

const SubmenuButtonInner = ({ content, handleAction }) => {
  const ref = useRef(null);
  const isPrimary = content.submenu_button?.style === "primary";
  const onCommand = useCallback(
    event => {
      let { config } = event.target;
      let mockEvent = {
        currentTarget: ref.current,
        source: config.id,
        name: "command",
        action: config.action,
      };
      handleAction(mockEvent);
    },
    [handleAction]
  );
  const onClick = useCallback(() => {
    let button = ref.current;
    let submenu = button?.querySelector(".fxms-multi-stage-submenu");
    if (submenu && !button.hasAttribute("open")) {
      submenu.openPopup(button, { position: "after_end" });
    }
  }, []);
  useEffect(() => {
    let button = ref.current;
    if (!button || button.querySelector(".fxms-multi-stage-submenu")) {
      return null;
    }
    let menupopup = document.createXULElement("menupopup");
    menupopup.className = "fxms-multi-stage-submenu";
    addMenuitems(content.submenu_button.submenu, menupopup);
    button.appendChild(menupopup);
    let stylesheet;
    if (
      !document.head.querySelector(
        `link[href="chrome://global/content/widgets.css"], link[href="chrome://global/skin/global.css"]`
      )
    ) {
      stylesheet = document.createElement("link");
      stylesheet.rel = "stylesheet";
      stylesheet.href = "chrome://global/content/widgets.css";
      document.head.appendChild(stylesheet);
    }
    if (!menupopup.listenersRegistered) {
      menupopup.addEventListener("command", onCommand);
      menupopup.addEventListener("popupshowing", event => {
        if (event.target === menupopup && event.target.anchorNode) {
          event.target.anchorNode.toggleAttribute("open", true);
        }
      });
      menupopup.addEventListener("popuphiding", event => {
        if (event.target === menupopup && event.target.anchorNode) {
          event.target.anchorNode.toggleAttribute("open", false);
        }
      });
      menupopup.listenersRegistered = true;
    }
    return () => {
      menupopup?.remove();
      stylesheet?.remove();
    };
  }, [onCommand]); // eslint-disable-line react-hooks/exhaustive-deps

  return (
    <Localized text={content.submenu_button.label ?? {}}>
      <button
        className={`submenu-button ${isPrimary ? "primary" : "secondary"}`}
        value="submenu_button"
        onClick={onClick}
        ref={ref}
      />
    </Localized>
  );
};
