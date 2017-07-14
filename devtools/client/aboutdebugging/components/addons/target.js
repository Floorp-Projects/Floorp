/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { debugAddon, isTemporaryID, parseFileUri, uninstallAddon } =
  require("../../modules/addon");
const Services = require("Services");

loader.lazyImporter(this, "BrowserToolboxProcess",
  "resource://devtools/client/framework/ToolboxProcess.jsm");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/main", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const TEMP_ID_URL = "https://developer.mozilla.org/Add-ons" +
                    "/WebExtensions/WebExtensions_and_the_Add-on_ID";

function filePathForTarget(target) {
  // Only show file system paths, and only for temporarily installed add-ons.
  if (!target.temporarilyInstalled || !target.url || !target.url.startsWith("file://")) {
    return [];
  }
  let path = parseFileUri(target.url);
  return [
    dom.dt(
      { className: "addon-target-info-label" },
      Strings.GetStringFromName("location")),
    // Wrap the file path in a span so we can do some RTL/LTR swapping to get
    // the ellipsis on the left.
    dom.dd(
      { className: "addon-target-info-content file-path" },
      dom.span({ className: "file-path-inner", title: path }, path),
    ),
  ];
}

function internalIDForTarget(target) {
  if (!target.manifestURL) {
    return [];
  }
  // Strip off the protocol and rest, leaving us with just the UUID.
  let uuid = /moz-extension:\/\/([^/]*)/.exec(target.manifestURL)[1];
  return [
    dom.dt(
      { className: "addon-target-info-label" },
      Strings.GetStringFromName("internalUUID"),
    ),
    dom.dd(
      { className: "addon-target-info-content internal-uuid" },
      dom.span(
        { title: uuid },
        uuid
      ),
      dom.span(
        { className: "addon-target-info-more" },
        dom.a(
          { href: target.manifestURL, target: "_blank", className: "manifest-url" },
          Strings.GetStringFromName("manifestURL"),
        ),
      )
    ),
  ];
}

function temporaryID(target) {
  if (!isTemporaryID(target.addonID)) {
    return [];
  }

  return [
    dom.div({ className: "addons-tip" },
      dom.span({ className: "addons-web-ext-tip" },
        Strings.GetStringFromName("temporaryID")
      ),
      dom.a({ href: TEMP_ID_URL, className: "temporary-id-url", target: "_blank" },
        Strings.GetStringFromName("temporaryID.learnMore")
      )
    )
  ];
}

module.exports = createClass({
  displayName: "AddonTarget",

  propTypes: {
    client: PropTypes.instanceOf(DebuggerClient).isRequired,
    debugDisabled: PropTypes.bool,
    target: PropTypes.shape({
      addonActor: PropTypes.string.isRequired,
      addonID: PropTypes.string.isRequired,
      icon: PropTypes.string,
      name: PropTypes.string.isRequired,
      temporarilyInstalled: PropTypes.bool,
      url: PropTypes.string,
    }).isRequired
  },

  debug() {
    let { target } = this.props;
    debugAddon(target.addonID);
  },

  uninstall() {
    let { target } = this.props;
    uninstallAddon(target.addonID);
  },

  reload() {
    let { client, target } = this.props;
    // This function sometimes returns a partial promise that only
    // implements then().
    client.request({
      to: target.addonActor,
      type: "reload"
    }).then(() => {}, error => {
      throw new Error(
        "Error reloading addon " + target.addonID + ": " + error);
    });
  },

  render() {
    let { target, debugDisabled } = this.props;

    return dom.li(
      { className: "addon-target-container", "data-addon-id": target.addonID },
      dom.div({ className: "target" },
        dom.img({
          className: "target-icon",
          role: "presentation",
          src: target.icon
        }),
        dom.span({ className: "target-name", title: target.name }, target.name)
      ),
      ...temporaryID(target),
      dom.dl(
        { className: "addon-target-info" },
        ...filePathForTarget(target),
        ...internalIDForTarget(target),
      ),
      dom.div({className: "addon-target-actions"},
        dom.button({
          className: "debug-button addon-target-button",
          onClick: this.debug,
          disabled: debugDisabled,
        }, Strings.GetStringFromName("debug")),
        target.temporarilyInstalled
          ? dom.button({
            className: "reload-button addon-target-button",
            onClick: this.reload,
          }, Strings.GetStringFromName("reload"))
          : null,
        target.temporarilyInstalled
          ? dom.button({
            className: "uninstall-button addon-target-button",
            onClick: this.uninstall,
          }, Strings.GetStringFromName("remove"))
          : null,
      ),
    );
  }
});
