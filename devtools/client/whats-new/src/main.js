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
  href: `https://support.mozilla.org/kb/send-tab-firefox-other-devices?${utmParams}`,
};

const release = {
  title: "What’s New in DevTools (Firefox 72 & 71)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/72?${utmParams}`,
  features: [
    {
      header: `Debug Variables with Watchpoints`,
      description: `Debugger’s new Watchpoint feature lets you pause when properties get read or write. Right-click object properties in the Scopes pane when paused to use the new “Break on…” menu.`,
      href: `https://wiki.developer.mozilla.org/docs/Tools/Debugger/How_to/Set_a_watchpoint_on_a_property?${utmParams}`,
    },
    {
      header: `Improvements to VS Code’s Debugger for Firefox`,
      description: `A new source map integration makes debugging faster and also integrates with VSCode’s new column breakpoints UI for fine-grained control. The new Watchpoints can be used via VSCode’s Data Points.`,
      href: `https://marketplace.visualstudio.com/items?itemName=firefox-devtools.vscode-firefox-debug`,
    },
    {
      header: `Async Stacks for Console traces`,
      description: `Stack Traces in the Console include the full chain of asynchronous promises and callbacks over time, including timeouts and events.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/Console_messages?${utmParams}`,
    },
    {
      header: `New in 71: New WebSocket Messages Inspector`,
      description: `The Network panel has a new Messages tab for inspecting WebSocket frames sent and received through the selected connection.`,
      href: `https://hacks.mozilla.org/2019/10/firefoxs-new-websocket-inspector/?${utmParams}`,
    },
    {
      header: `New in 71: Block URLs in the Network panel`,
      description: `Test how a page loads without specific files, like CSS or JavaScript. Right-click network requests and select “Block URL” or use the new Request Blocking pane.`,
      href: `https://developer.mozilla.org/docs/Tools/Network_Monitor/request_list?${utmParams}#Block_a_specific_URL`,
    },
    {
      header: `New in 71: Log on Events`,
      description: `Enabling logging for Event Listener Breakpoints in the Debugger lets you observe which event handlers are being executed without the overhead of pausing.`,
      href: `https://developer.mozilla.org/docs/Tools/Debugger/Set_event_listener_breakpoints?${utmParams}`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl: `https://www.mozilla.org/firefox/developer/?${utmParams}`,
  linkText: "Get DevEdition",
  features: [
    {
      header: `Asynchronous Stacks in Debugger`,
      description: `Asynchronous call stacks in the Debugger let you examine the event-driven function calls at previous points in time.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/UI_Tour?${utmParams}`,
    },
    {
      header: `Correct Meta Viewport in Responsive Design Mode`,
      description: `RDM with touch simulation enabled will now correctly simulate the meta viewport rendering of a mobile device.`,
      href: `https://developer.mozilla.org/docs/Tools/Responsive_Design_Mode?${utmParams}`,
    },
    {
      header: `CSP Style Directives No Longer Prevent CSS Edits`,
      description: `Editing an element’s inline style from Inspector’s rule-view now works even if style-src is blocked by CSP.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector/How_to/Examine_and_edit_CSS?${utmParams}`,
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
