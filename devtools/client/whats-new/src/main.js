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
  title: "What’s New in DevTools (Firefox 74 & 73)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/74?${utmParams}`,
  features: [
    {
      header: `Debugging Async/Await Stacks`,
      description: `Awaiting call stacks in the Debugger allow you to examine the promise-driven function calls at previous points in time.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/UI_Tour?${utmParams}`,
    },
    {
      header: `Optional chaining in Console`,
      description: `Freshly landed support for the optional chaining operator "?." which can also be used with Console's autocomplete.`,
      href: `https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/Optional_chaining?${utmParams}`,
    },
    {
      header: `New in 73: More WebSocket protocols`,
      description: `WAMP-encoded data is now decoded into a readable payload (on top of SignalR, Socket.IO, etc) to make it easier to debug various WebSocket message formats.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/Inspecting_web_sockets?${utmParams}#Supported_WS_protocols`,
    },
    {
      header: `New in 73: CORS Network Errors`,
      description: `To make CORS network errors harder to miss, Console now shows them as errors and not warnings.`,
      href: `https://wiki.developer.mozilla.org/en-US/docs/Web/HTTP/CORS/Errors?${utmParams}`,
    },
    {
      header: `New in 73: Performance Highlights`,
      description: `We made fast-firing requests in Network panel and large JavaScript source maps in Debugger a lot snappier.`,
      href: `https://hacks.mozilla.org/2020/02/firefox-73-is-upon-us/?${utmParams}`,
    },
    {
      header: `New in 73: CSP Style Directives No Longer Prevent CSS Edits`,
      description: `Editing an element’s inline style from Inspector’s rule-view now works even if style-src is blocked by CSP.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector/How_to/Examine_and_edit_CSS?${utmParams}`,
    },
    {
      header: `New in 72: Correct Meta Viewport in Responsive Design Mode`,
      description: `RDM with touch simulation enabled will now correctly simulate the meta viewport rendering of a mobile device.`,
      href: `https://developer.mozilla.org/docs/Tools/Responsive_Design_Mode?${utmParams}`,
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
    {
      header: `Inspect & Debug Service Workers`,
      description: `Start, pause and debug your Service Workers and inspect your Web App Manifests within the long-awaited Application panel.`,
      href: `https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API/Using_Service_Workers?${utmParams}`,
    },
    {
      header: `Instantly evaluated Console previews`,
      description: `Peek into the results of your current Console expression and autocomplete values as you type.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/The_command_line_interpreter?${utmParams}`,
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
