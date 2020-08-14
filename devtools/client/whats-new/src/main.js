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
  title: "What’s New in DevTools (Firefox 80)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/80?${utmParams}`,
  features: [
    {
      header: `New commands for HTTP request blocking`,
      description: `You can now block and unblock network requests using the :block and :unblock helper commands in the Console.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Web_Console/Helpers?${utmParams}`,
    },
    {
      header: `Class autocomplete in HTML editing`,
      description: `When adding a class to an element in the Page Inspector's Rules pane, existing classes are suggested with autocomplete.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Page_Inspector/How_to/Examine_and_edit_CSS#Viewing_and_changing_classes_on_an_element?${utmParams}`,
    },
    {
      header: `Stack traces for exceptions`,
      description: `When the Debugger breaks on an exception, the tooltip in the source pane now shows disclosure triangle that reveals a stack trace.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Breaking_on_exceptions?${utmParams}`,
    },
    {
      header: `Warning for slow server responses`,
      description: `In the Network Monitor request list, a turtle icon is shown for "slow" requests that exceed a configurable threshhold for the waiting time.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor/request_list#Network_request_columns?${utmParams}`,
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
      description: `Monitor your received event stream data, in addition to the existing WebSocket inspection. Flip "devtools.netmonitor.features.serverSentEvents" pref to try this feature.`,
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
