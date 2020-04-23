/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Types = require("devtools/client/shared/vendor/react-prop-types");
const { openDocLink } = require("devtools/client/shared/link");
var Services = require("Services");
const isMacOS = Services.appinfo.OS === "Darwin";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const utmParams = new URLSearchParams({
  utm_source: "devtools",
  utm_medium: "devtools_whatsnew",
});

const aside = {
  header: "Instantly Send Tabs to Mobile",
  content: `Test your site on mobile and other devices without having to copy, paste, or leave the browser.`,
  cta: "Learn More About Send Tabs",
  href: `https://play.google.com/store/apps/details?id=org.mozilla.fennec_aurora&referrer=utm_source%3Dmozilla%26utm_medium%3DReferral%26utm_campaign%3Dmozilla-org${utmParams}`,
};

const release = {
  title: "What’s New in DevTools (Firefox 76)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/76?${utmParams}`,
  features: [
    {
      header: `Ignore entire folders in Debugger`,
      description: `Filter out the noise of extraneous groups of sources with a new “Blackbox” context menu in Debugger’s sources list. Ignoring can be limited to files inside or outside of the selected folder. Combine with “Set directory root” for a laser-focused debugging experience.`,
      href: `https://wiki.developer.mozilla.org/en-US/docs/Tools/Debugger/UI_Tour?${utmParams}#Source_list_pane`,
    },
    {
      header: `Console collapses multi-line code snippets`,
      description: `Console’s multi-line editor mode just got better to iterate on long snippets with less clutter. Multiple lines are neatly collapsed and can be expanded on demand.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/The_command_line_interpreter?${utmParams}#Multi-line_mode`,
    },
    {
      header: `Formatted Action Cable WebSocket messages`,
      description: `The JSON embedded in Action Cable messages are now broken out to be more readable, adding to a growing set of supported WebSocket protocol formats.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/Inspecting_web_sockets?${utmParams}`,
    },
    {
      header: `Cleaner WebSocket output`,
      description: `WebSocket control frames are now hidden by default to let you focus on the content actual send and received messages.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/Inspecting_web_sockets?${utmParams}`,
    },
    {
      header: `Resize Network table columns to fit to content`,
      description: `Expanding longer content in the Network panel no longer requires countless dragging and resizing. Like modern data tables, just double-tap the table’s resize handles to fit the column size to its content.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list?${utmParams}`,
    },
    {
      header: `Improve Network response details`,
      description: `Response data is now much easier to navigate and copy out of the Network details. More work is coming in this area to make various kinds of Network analysis easier.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_details?${utmParams}`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl: `https://www.mozilla.org/firefox/developer/?${utmParams}`,
  linkText: "Get DevEdition",
  features: [
    {
      header: `Full Asynchronous Stacks in Debugger`,
      description: `Step through event, timeout and promise-based function calls over time with the full-featured async stacks in Debugger.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/UI_Tour?${utmParams}`,
    },
  ],
};

function openLink(href, e) {
  return openDocLink(href, {
    relatedToCurrent: true,
    inBackground: isMacOS ? e.metaKey : e.ctrlKey,
  });
}

class Aside extends Component {
  render() {
    return dom.aside(
      {},
      dom.div(
        { className: "call-out" },
        dom.h3({}, aside.header),
        dom.p({}, aside.content),
        dom.p(
          { className: "cta" },
          dom.a(
            {
              href: aside.href,
              className: "devtools-button",
              onClick: e => {
                e.preventDefault();
                openLink(aside.href, e);
              },
            },
            aside.cta
          )
        )
      )
    );
  }
}

class Feature extends Component {
  static get propTypes() {
    return {
      header: Types.string,
      description: Types.string,
      href: Types.string,
    };
  }

  render() {
    const { header, description, href } = this.props;
    return dom.li(
      { key: header, className: "feature" },
      dom.a(
        {
          href,
          onClick: e => {
            e.preventDefault();
            openLink(href, e);
          },
        },
        dom.h3({}, header),
        dom.p({}, description)
      )
    );
  }
}

function Link(text, href) {
  return dom.a(
    {
      className: "link",
      href,
      onClick: e => {
        e.preventDefault();
        openLink(href, e);
      },
    },
    text
  );
}

class App extends Component {
  render() {
    return dom.main(
      {},
      createFactory(Aside)(),
      dom.article(
        {},
        dom.h2(
          {},
          dom.span({}, release.title),
          Link(release.linkText, release.linkUrl)
        ),
        dom.ul(
          {},
          ...release.features
            .filter(feature => !feature.hidden)
            .map(feature => createFactory(Feature)(feature))
        ),
        dom.h2({}, dom.span({}, dev.title), Link(dev.linkText, dev.linkUrl)),
        dom.ul(
          {},
          ...dev.features.map(feature => createFactory(Feature)(feature))
        )
      )
    );
  }
}

module.exports = {
  bootstrap: () => {
    const root = document.querySelector("#root");
    ReactDOM.render(createFactory(App)(), root);
  },
  destroy: () => {
    const root = document.querySelector("#root");
    ReactDOM.unmountComponentAtNode(root);
  },
};
