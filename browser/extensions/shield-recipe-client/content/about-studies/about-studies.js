/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* global classnames PropTypes r React ReactDOM ShieldStudies */

/**
 * Mapping of pages displayed on the sidebar. Keys are the value used in the
 * URL hash to identify the current page.
 *
 * Pages will appear in the sidebar in the order they are defined here. If the
 * URL doesn't contain a hash, the first page will be displayed in the content area.
 */
const PAGES = new Map([
  ["shieldStudies", {
    name: "Shield Studies",
    component: ShieldStudies,
    icon: "resource://shield-recipe-client-content/about-studies/img/shield-logo.png",
  }],
]);

/**
 * Handle basic layout and routing within about:studies.
 */
class AboutStudies extends React.Component {
  constructor(props) {
    super(props);

    let hash = new URL(window.location).hash.slice(1);
    if (!PAGES.has(hash)) {
      hash = "shieldStudies";
    }

    this.state = {
      currentPageId: hash,
    };

    this.handleEvent = this.handleEvent.bind(this);
  }

  componentDidMount() {
    window.addEventListener("hashchange", this);
  }

  componentWillUnmount() {
    window.removeEventListener("hashchange", this);
  }

  handleEvent(event) {
    const newHash = new URL(event.newURL).hash.slice(1);
    if (PAGES.has(newHash)) {
      this.setState({currentPageId: newHash});
    }
  }

  render() {
    const currentPageId = this.state.currentPageId;
    const pageEntries = Array.from(PAGES.entries());
    const currentPage = PAGES.get(currentPageId);

    return (
      r("div", {className: "about-studies-container"},
        r(Sidebar, {},
          pageEntries.map(([id, page]) => (
            r(SidebarItem, {
              key: id,
              pageId: id,
              selected: id === currentPageId,
              page,
            })
          )),
        ),
        r(Content, {},
          currentPage && r(currentPage.component)
        ),
      )
    );
  }
}

class Sidebar extends React.Component {
  render() {
    return r("ul", {id: "categories"}, this.props.children);
  }
}
Sidebar.propTypes = {
  children: PropTypes.node,
};

class SidebarItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    window.location = `#${this.props.pageId}`;
  }

  render() {
    const { page, selected } = this.props;
    return (
      r("li", {
        className: classnames("category", {selected}),
        onClick: this.handleClick,
      },
        page.icon && r("img", {className: "category-icon", src: page.icon}),
        r("span", {className: "category-name"}, page.name),
      )
    );
  }
}
SidebarItem.propTypes = {
  pageId: PropTypes.string.isRequired,
  page: PropTypes.shape({
    icon: PropTypes.string,
    name: PropTypes.string.isRequired,
  }).isRequired,
  selected: PropTypes.bool,
};

class Content extends React.Component {
  render() {
    return (
      r("div", {className: "main-content"},
        r("div", {className: "content-box"},
          this.props.children,
        ),
      )
    );
  }
}
Content.propTypes = {
  children: PropTypes.node,
};

ReactDOM.render(
  r(AboutStudies),
  document.getElementById("app"),
);
