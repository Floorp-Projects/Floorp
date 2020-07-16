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
  header: "A Fast and Secure VPN",
  content: `A VPN from the trusted pioneer in internet privacy. Available for users in the US, Canada, UK, Singapore, Malaysia, and New Zealand.`,
  cta: "Try Mozilla VPN",
  href: `https://vpn.mozilla.org/?${utmParams}`,
};

const release = {
  title: "What’s New in DevTools (Firefox 79)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/79?${utmParams}`,
  features: [
    {
      header: `Async stack visibility in Console & Debugger`,
      description: `Now you can view async code events, timeouts, and promises in the context of their stacks across panels. Benefit from the additional context in Debugger call stacks, Network initiator stacks, and Console error and log stacks.`,
      href: `https://developer.mozilla.org/en-US/docs/Learn/JavaScript/Asynchronous/Timeouts_and_intervals?${utmParams}`,
    },
    {
      header: `Even better source map references for JS and CSS`,
      description: `Links to Style Editor and Debugger reliably jump to the right file and location. This results in a smoother SCSS inspection experience.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Use_a_source_map?${utmParams}`,
    },
    {
      header: `Console displays failed requests`,
      description: `Network responses with 4xx/5xx status codes are now easier to spot and debug in the Console. In Firefox 79, they display as errors and can be expanded to view detailed request and response information.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/Console_messages?${utmParams}`,
    },
    {
      header: `Debugger highlights errors in code`,
      description: `When JavaScript code throws an error, the debugger will now highlight the relevant line of code and display error details.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Breaking_on_exceptions?${utmParams}`,
    },
    {
      header: `Faster JavaScript debugging`,
      description: `Projects with large files and eval-heavy code patterns can now be debugged more quickly.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger?${utmParams}`,
    },
    {
      header: `More accessible Inspector`,
      description: `Inspector's Changes and Layout tabs can now be read by screen reader users.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector?${utmParams}`,
    },
    {
      header: `Disabling cache also includes CORS preflight requests`,
      description: `The "Disable Cache" option in the Network panel now also deactivates CORS preflight request caching, making it easier to iterate on your web security settings.`,
      href: `https://developer.mozilla.org/en-US/docs/Glossary/Preflight_request?${utmParams}`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl: `https://www.mozilla.org/firefox/developer/?${utmParams}`,
  linkText: "Get DevEdition",
  features: [
    {
      header: `Inspect Server-Sent Events`,
      description: `Monitor your received event stream data, adding to the existing WebSocket inspection.`,
      href: `https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events?${utmParams}`,
    },
    {
      header: `Warning for slow server responses`,
      description: `The Network panel now highlights server performance bottlenecks. Watch out for the friendly turtle icon.`,
      href: `https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events?${utmParams}`,
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
