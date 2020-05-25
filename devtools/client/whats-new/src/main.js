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
  title: "What’s New in DevTools (Firefox 77)",
  linkText: "Read more",
  linkUrl: `https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/77?${utmParams}`,
  features: [
    {
      header: `Faster, leaner debugging`,
      description: `Performance improvements that not only speed up pausing and stepping in the Debugger but also cut down on memory usage over time.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger?${utmParams}`,
    },
    {
      header: `Source Maps that just work`,
      description: `Firefox 77 has the most fixes yet to make source maps faster and more dependable so that your original CSS and JavaScript/TypeScript/etc code is always at hand. Especially improved: build outputs which previously failed to load source maps.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Use_a_source_map?${utmParams}`,
    },
    {
      header: `Step JavaScript in the selected stack frame`,
      description: `Stepping in the currently selected stack makes the debugging code execution flow more intuitive and makes it less likely for you to miss an important line.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Step_through_code?${utmParams}`,
    },
    {
      header: `Overflow settings for Network and Debugger`,
      description: `Making for a leaner toolbar, Network and Debugger follow Console’s example in combining existing and new checkboxes into a new settings menu. This puts powerful options like “Disable JavaScript” right at your fingertips.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Network_Monitor?${utmParams}`,
    },
    {
      header: `Pause on property read & write`,
      description: `Understanding state changes is core to debugging. Pausing when a script references a property is now easier with a new Watchpoints option that combines get/set.`,
      href: `https://developer.mozilla.org/en-US/docs/Tools/Debugger/How_to/Use_watchpoints?${utmParams}`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl: `https://www.mozilla.org/firefox/developer/?${utmParams}`,
  linkText: "Get DevEdition",
  features: [
    {
      header: `Cross-browser CSS compatibility audit`,
      description: `Analyze your CSS for cross-browser compatibility with a new sidepanel in the Inspector. Let us know how well it worked for your projects via the Feedback option!`,
      href: `https://discourse.mozilla.org/t/new-in-devedition-77-css-compatibility-in-inspector/60669`,
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
