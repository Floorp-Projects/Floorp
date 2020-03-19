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
  title: "What’s New in DevTools (Firefox 75)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/75?${utmParams}`,
  features: [
    {
      header: `Instant evaluation for Console expressions`,
      description: `Identify and fix errors more rapidly than before. As long as expressions typed into the Web Console are side-effect free, their results will be previewed while you type.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/The_command_line_interpreter?${utmParams}`,
    },
    {
      header: `Measurement area is now resizable`,
      description: `Quickly drag the area on the fly to measure the height, width and diagonal in your page. Enable the feature in Setting, under "Available Toolbox Buttons".`,
      href: `https://developer.mozilla.org/docs/Tools/Measure_a_portion_of_the_page?${utmParams}`,
    },
    {
      header: `Event breakpoints for WebSockets`,
      description: `Newly added event breakpoint types in the Debugger let you pause or log when WebSocket event handlers get executed.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/Set_event_listener_breakpoints?${utmParams}`,
    },
    {
      header: `Use XPath to find DOM elements`,
      description: `XPath queries, common in automation tools, can now be used in Inspector’s HTML search to test expressions and find matching elements.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector/How_to/Examine_and_edit_HTML?${utmParams}#XPath_search`,
    },
    {
      header: `Polished Network analysis`,
      description: `Optimized table rendering renders many simultaneous requests much faster. Borders between columns and higher contrast state for filter buttons makes reading easier.`,
      href: `https://wiki.developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list?${utmParams}`,
    },
    {
      header: `Request blocking with “*” wildcards`,
      description: `Use URL patterns with * wildcards in Network’s Request Blocking panel to test how resilient your site is when matches requests fail.`,
      href: `https://wiki.developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list?${utmParams}#Blocking_specific_URLs`,
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
