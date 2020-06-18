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
  title: "What’s New in DevTools (Firefox 78)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/77?${utmParams}`,
  features: [
    {
      header: `Better logs for uncaught promise rejections`,
      description: `Framework and promise errors are now logged with detailed error stacks, names, and properties, especially improving Angular debugging.`,
      href: `https://developer.mozilla.org/en-US/docs/Web/JavaScript/Guide/Using_promises?${utmParams}#Promise_rejection_events`,
    },
    {
      header: `Faster DOM navigation in Inspector`,
      description: `Loading and navigating the DOM on sites with many CSS variables is much snappier.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector?${utmParams}`,
    },
    {
      header: `Source-mapped variables in Logpoints`,
      description: `Logpoints can log any variable in original and source-mapped files by automatically finding references to minified names. Make sure to enable the "Map" option in Debugger’s "Scopes" pane.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/Set_a_logpoint?${utmParams}`,
    },
    {
      header: `Better support for blocked network requests`,
      description: `Network requests blocked by Enhanced Tracking Protection, add-ons, and CORS are now captured in the Network panel and include detailed blocking reasons.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list?${utmParams}`,
    },
    {
      header: `URL bar & reload in Remote Debugging`,
      description: `Navigate your mobile Firefox browser from the remote debugging UI, avoiding fiddling on tiny device screens.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list?${utmParams}`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl: `https://www.mozilla.org/firefox/developer/?${utmParams}`,
  linkText: "Get DevEdition",
  features: [
    {
      header: `Async stacks in Console & Debugger`,
      description: `Step through event, timeout, and promise-based function calls over time with the full-featured async stacks in Debugger.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/UI_Tour?${utmParams}#Call_stack`,
    },
    {
      header: `Correct references to source-mapped JS/CSS`,
      description: `Source-mapped file references in the Inspector and Console now link more reliably to the right file.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Use_a_source_map?${utmParams}`,
    },
    {
      header: `Console shows failed requests`,
      description: `Network requests with 4xx/5xx status codes now log as errors in the Console and can be inspected using the embedded network details.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console?${utmParams}`,
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
