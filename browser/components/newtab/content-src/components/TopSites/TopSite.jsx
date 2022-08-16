/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";
import {
  MIN_RICH_FAVICON_SIZE,
  MIN_SMALL_FAVICON_SIZE,
  TOP_SITES_CONTEXT_MENU_OPTIONS,
  TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS,
  TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS,
  TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS,
  TOP_SITES_SOURCE,
} from "./TopSitesConstants";
import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import { ImpressionStats } from "../DiscoveryStreamImpressionStats/ImpressionStats";
import React from "react";
import { ScreenshotUtils } from "content-src/lib/screenshot-utils";
import { TOP_SITES_MAX_SITES_PER_ROW } from "common/Reducers.jsm";
import { ContextMenuButton } from "content-src/components/ContextMenu/ContextMenuButton";
import { TopSiteImpressionWrapper } from "./TopSiteImpressionWrapper";
const SPOC_TYPE = "SPOC";
const NEWTAB_SOURCE = "newtab";

export class TopSiteLink extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { screenshotImage: null };
    this.onDragEvent = this.onDragEvent.bind(this);
    this.onKeyPress = this.onKeyPress.bind(this);
  }

  /*
   * Helper to determine whether the drop zone should allow a drop. We only allow
   * dropping top sites for now. We don't allow dropping on sponsored top sites
   * as their position is fixed.
   */
  _allowDrop(e) {
    return (
      (this.dragged || !this.props.link.sponsored_position) &&
      e.dataTransfer.types.includes("text/topsite-index")
    );
  }

  onDragEvent(event) {
    switch (event.type) {
      case "click":
        // Stop any link clicks if we started any dragging
        if (this.dragged) {
          event.preventDefault();
        }
        break;
      case "dragstart":
        event.target.blur();
        if (this.props.link.sponsored_position) {
          event.preventDefault();
          break;
        }
        this.dragged = true;
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/topsite-index", this.props.index);
        this.props.onDragEvent(
          event,
          this.props.index,
          this.props.link,
          this.props.title
        );
        break;
      case "dragend":
        this.props.onDragEvent(event);
        break;
      case "dragenter":
      case "dragover":
      case "drop":
        if (this._allowDrop(event)) {
          event.preventDefault();
          this.props.onDragEvent(event, this.props.index);
        }
        break;
      case "mousedown":
        // Block the scroll wheel from appearing for middle clicks on search top sites
        if (event.button === 1 && this.props.link.searchTopSite) {
          event.preventDefault();
        }
        // Reset at the first mouse event of a potential drag
        this.dragged = false;
        break;
    }
  }

  /**
   * Helper to obtain the next state based on nextProps and prevState.
   *
   * NOTE: Rename this method to getDerivedStateFromProps when we update React
   *       to >= 16.3. We will need to update tests as well. We cannot rename this
   *       method to getDerivedStateFromProps now because there is a mismatch in
   *       the React version that we are using for both testing and production.
   *       (i.e. react-test-render => "16.3.2", react => "16.2.0").
   *
   * See https://github.com/airbnb/enzyme/blob/master/packages/enzyme-adapter-react-16/package.json#L43.
   */
  static getNextStateFromProps(nextProps, prevState) {
    const { screenshot } = nextProps.link;
    const imageInState = ScreenshotUtils.isRemoteImageLocal(
      prevState.screenshotImage,
      screenshot
    );
    if (imageInState) {
      return null;
    }

    // Since image was updated, attempt to revoke old image blob URL, if it exists.
    ScreenshotUtils.maybeRevokeBlobObjectURL(prevState.screenshotImage);

    return {
      screenshotImage: ScreenshotUtils.createLocalImageObject(screenshot),
    };
  }

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillMount() {
    const nextState = TopSiteLink.getNextStateFromProps(this.props, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  // NOTE: Remove this function when we update React to >= 16.3 since React will
  //       call getDerivedStateFromProps automatically. We will also need to
  //       rename getNextStateFromProps to getDerivedStateFromProps.
  componentWillReceiveProps(nextProps) {
    const nextState = TopSiteLink.getNextStateFromProps(nextProps, this.state);
    if (nextState) {
      this.setState(nextState);
    }
  }

  componentWillUnmount() {
    ScreenshotUtils.maybeRevokeBlobObjectURL(this.state.screenshotImage);
  }

  onKeyPress(event) {
    // If we have tabbed to a search shortcut top site, and we click 'enter',
    // we should execute the onClick function. This needs to be added because
    // search top sites are anchor tags without an href. See bug 1483135
    if (this.props.link.searchTopSite && event.key === "Enter") {
      this.props.onClick(event);
    }
  }

  /*
   * Takes the url as a string, runs it through a simple (non-secure) hash turning it into a random number
   * Apply that random number to the color array. The same url will always generate the same color.
   */
  generateColor() {
    let { title, colors } = this.props;
    if (!colors) {
      return "";
    }

    let colorArray = colors.split(",");

    const hashStr = str => {
      let hash = 0;
      for (let i = 0; i < str.length; i++) {
        let charCode = str.charCodeAt(i);
        hash += charCode;
      }
      return hash;
    };

    let hash = hashStr(title);
    let index = hash % colorArray.length;
    return colorArray[index];
  }

  calculateStyle() {
    const { defaultStyle, link } = this.props;

    const { tippyTopIcon, faviconSize } = link;
    let imageClassName;
    let imageStyle;
    let showSmallFavicon = false;
    let smallFaviconStyle;
    let hasScreenshotImage =
      this.state.screenshotImage && this.state.screenshotImage.url;
    let selectedColor;

    if (defaultStyle) {
      // force no styles (letter fallback) even if the link has imagery
      selectedColor = this.generateColor();
    } else if (link.searchTopSite) {
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon})`,
      };
      smallFaviconStyle = { backgroundImage: `url(${tippyTopIcon})` };
    } else if (link.customScreenshotURL) {
      // assume high quality custom screenshot and use rich icon styles and class names

      // TopSite spoc experiment only
      const spocImgURL =
        link.type === SPOC_TYPE ? link.customScreenshotURL : "";

      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: hasScreenshotImage
          ? `url(${this.state.screenshotImage.url})`
          : `url(${spocImgURL})`,
      };
    } else if (tippyTopIcon || faviconSize >= MIN_RICH_FAVICON_SIZE) {
      // styles and class names for top sites with rich icons
      imageClassName = "top-site-icon rich-icon";
      imageStyle = {
        backgroundColor: link.backgroundColor,
        backgroundImage: `url(${tippyTopIcon || link.favicon})`,
      };
    } else if (faviconSize >= MIN_SMALL_FAVICON_SIZE) {
      showSmallFavicon = true;
      smallFaviconStyle = { backgroundImage: `url(${link.favicon})` };
    } else {
      selectedColor = this.generateColor();
      imageClassName = "";
    }

    return {
      showSmallFavicon,
      smallFaviconStyle,
      imageStyle,
      imageClassName,
      selectedColor,
    };
  }

  render() {
    const {
      children,
      className,
      isDraggable,
      link,
      onClick,
      title,
    } = this.props;
    const topSiteOuterClassName = `top-site-outer${
      className ? ` ${className}` : ""
    }${link.isDragged ? " dragged" : ""}${
      link.searchTopSite ? " search-shortcut" : ""
    }`;
    const [letterFallback] = title;
    const {
      showSmallFavicon,
      smallFaviconStyle,
      imageStyle,
      imageClassName,
      selectedColor,
    } = this.calculateStyle();

    let draggableProps = {};
    if (isDraggable) {
      draggableProps = {
        onClick: this.onDragEvent,
        onDragEnd: this.onDragEvent,
        onDragStart: this.onDragEvent,
        onMouseDown: this.onDragEvent,
      };
    }

    return (
      <li
        className={topSiteOuterClassName}
        onDrop={this.onDragEvent}
        onDragOver={this.onDragEvent}
        onDragEnter={this.onDragEvent}
        onDragLeave={this.onDragEvent}
        {...draggableProps}
      >
        <div className="top-site-inner">
          {/* We don't yet support an accessible drag-and-drop implementation, see Bug 1552005 */}
          {/* eslint-disable-next-line jsx-a11y/anchor-is-valid */}
          <a
            className="top-site-button"
            href={link.searchTopSite ? undefined : link.url}
            tabIndex="0"
            onKeyPress={this.onKeyPress}
            onClick={onClick}
            draggable={true}
            data-is-sponsored-link={!!link.sponsored_tile_id}
          >
            <div className="tile" aria-hidden={true}>
              <div
                className={
                  selectedColor
                    ? "icon-wrapper letter-fallback"
                    : "icon-wrapper"
                }
                data-fallback={letterFallback}
                style={selectedColor ? { backgroundColor: selectedColor } : {}}
              >
                <div className={imageClassName} style={imageStyle} />
                {showSmallFavicon && (
                  <div
                    className="top-site-icon default-icon"
                    data-fallback={smallFaviconStyle ? "" : letterFallback}
                    style={smallFaviconStyle}
                  />
                )}
              </div>
              {link.searchTopSite && (
                <div className="top-site-icon search-topsite" />
              )}
            </div>
            <div
              className={`title${link.isPinned ? " has-icon pinned" : ""}${
                link.type === SPOC_TYPE || link.show_sponsored_label
                  ? " sponsored"
                  : ""
              }`}
            >
              <span dir="auto">
                {link.isPinned && <div className="icon icon-pin-small" />}
                {title || <br />}
                <span
                  className="sponsored-label"
                  data-l10n-id="newtab-topsite-sponsored"
                />
              </span>
            </div>
          </a>
          {children}
          {link.type === SPOC_TYPE ? (
            <ImpressionStats
              flightId={link.flightId}
              rows={[
                {
                  id: link.id,
                  pos: link.pos,
                  shim: link.shim && link.shim.impression,
                },
              ]}
              dispatch={this.props.dispatch}
              source={TOP_SITES_SOURCE}
            />
          ) : null}
          {/* Set up an impression wrapper for the sponsored TopSite */}
          {link.sponsored_position ? (
            <TopSiteImpressionWrapper
              tile={{
                position: this.props.index + 1,
                tile_id: link.sponsored_tile_id || -1,
                reporting_url: link.sponsored_impression_url,
                advertiser: title.toLocaleLowerCase(),
                source: NEWTAB_SOURCE,
              }}
              dispatch={this.props.dispatch}
            />
          ) : null}
        </div>
      </li>
    );
  }
}
TopSiteLink.defaultProps = {
  title: "",
  link: {},
  isDraggable: true,
};

export class TopSite extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = { showContextMenu: false };
    this.onLinkClick = this.onLinkClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
  }

  /**
   * Report to telemetry additional information about the item.
   */
  _getTelemetryInfo() {
    const value = { icon_type: this.props.link.iconType };
    // Filter out "not_pinned" type for being the default
    if (this.props.link.isPinned) {
      value.card_type = "pinned";
    }
    if (this.props.link.searchTopSite) {
      // Set the card_type as "search" regardless of its pinning status
      value.card_type = "search";
      value.search_vendor = this.props.link.hostname;
    }
    if (
      this.props.link.type === SPOC_TYPE ||
      this.props.link.sponsored_position
    ) {
      value.card_type = "spoc";
    }
    return { value };
  }

  userEvent(event) {
    this.props.dispatch(
      ac.UserEvent(
        Object.assign(
          {
            event,
            source: TOP_SITES_SOURCE,
            action_position: this.props.index,
          },
          this._getTelemetryInfo()
        )
      )
    );
  }

  onLinkClick(event) {
    this.userEvent("CLICK");

    // Specially handle a top site link click for "typed" frecency bonus as
    // specified as a property on the link.
    event.preventDefault();
    const { altKey, button, ctrlKey, metaKey, shiftKey } = event;
    if (!this.props.link.searchTopSite) {
      this.props.dispatch(
        ac.OnlyToMain({
          type: at.OPEN_LINK,
          data: Object.assign(this.props.link, {
            event: { altKey, button, ctrlKey, metaKey, shiftKey },
          }),
        })
      );

      // Fire off a spoc specific impression.
      if (this.props.link.type === SPOC_TYPE) {
        this.props.dispatch(
          ac.ImpressionStats({
            source: TOP_SITES_SOURCE,
            click: 0,
            tiles: [
              {
                id: this.props.link.id,
                pos: this.props.link.pos,
                shim: this.props.link.shim && this.props.link.shim.click,
              },
            ],
          })
        );
      }
      if (this.props.link.sendAttributionRequest) {
        this.props.dispatch(
          ac.OnlyToMain({
            type: at.PARTNER_LINK_ATTRIBUTION,
            data: {
              targetURL: this.props.link.url,
              source: "newtab",
            },
          })
        );
      }
      if (this.props.link.sponsored_position) {
        const title = this.props.link.label || this.props.link.hostname;
        this.props.dispatch(
          ac.OnlyToMain({
            type: at.TOP_SITES_IMPRESSION_STATS,
            data: {
              type: "click",
              position: this.props.index + 1,
              tile_id: this.props.link.sponsored_tile_id || -1,
              reporting_url: this.props.link.sponsored_click_url,
              advertiser: title.toLocaleLowerCase(),
              source: NEWTAB_SOURCE,
            },
          })
        );
      }
    } else {
      this.props.dispatch(
        ac.OnlyToMain({
          type: at.FILL_SEARCH_TERM,
          data: { label: this.props.link.label },
        })
      );
    }
  }

  onMenuUpdate(isOpen) {
    if (isOpen) {
      this.props.onActivate(this.props.index);
    } else {
      this.props.onActivate();
    }
  }

  render() {
    const { props } = this;
    const { link } = props;
    const isContextMenuOpen = props.activeIndex === props.index;
    const title = link.label || link.hostname;
    let menuOptions;
    if (link.sponsored_position) {
      menuOptions = TOP_SITES_SPONSORED_POSITION_CONTEXT_MENU_OPTIONS;
    } else if (link.searchTopSite) {
      menuOptions = TOP_SITES_SEARCH_SHORTCUTS_CONTEXT_MENU_OPTIONS;
    } else if (link.type === SPOC_TYPE) {
      menuOptions = TOP_SITES_SPOC_CONTEXT_MENU_OPTIONS;
    } else {
      menuOptions = TOP_SITES_CONTEXT_MENU_OPTIONS;
    }

    return (
      <TopSiteLink
        {...props}
        onClick={this.onLinkClick}
        onDragEvent={this.props.onDragEvent}
        className={`${props.className || ""}${
          isContextMenuOpen ? " active" : ""
        }`}
        title={title}
      >
        <div>
          <ContextMenuButton
            tooltip="newtab-menu-content-tooltip"
            tooltipArgs={{ title }}
            onUpdate={this.onMenuUpdate}
          >
            <LinkMenu
              dispatch={props.dispatch}
              index={props.index}
              onUpdate={this.onMenuUpdate}
              options={menuOptions}
              site={link}
              shouldSendImpressionStats={link.type === SPOC_TYPE}
              siteInfo={this._getTelemetryInfo()}
              source={TOP_SITES_SOURCE}
            />
          </ContextMenuButton>
        </div>
      </TopSiteLink>
    );
  }
}
TopSite.defaultProps = {
  link: {},
  onActivate() {},
};

export class TopSitePlaceholder extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onEditButtonClick = this.onEditButtonClick.bind(this);
  }

  onEditButtonClick() {
    this.props.dispatch({
      type: at.TOP_SITES_EDIT,
      data: { index: this.props.index },
    });
  }

  render() {
    return (
      <TopSiteLink
        {...this.props}
        className={`placeholder ${this.props.className || ""}`}
        isDraggable={false}
      >
        <button
          aria-haspopup="true"
          className="context-menu-button edit-button icon"
          data-l10n-id="newtab-menu-topsites-placeholder-tooltip"
          onClick={this.onEditButtonClick}
        />
      </TopSiteLink>
    );
  }
}

export class TopSiteList extends React.PureComponent {
  static get DEFAULT_STATE() {
    return {
      activeIndex: null,
      draggedIndex: null,
      draggedSite: null,
      draggedTitle: null,
      topSitesPreview: null,
    };
  }

  constructor(props) {
    super(props);
    this.state = TopSiteList.DEFAULT_STATE;
    this.onDragEvent = this.onDragEvent.bind(this);
    this.onActivate = this.onActivate.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    if (this.state.draggedSite) {
      const prevTopSites = this.props.TopSites && this.props.TopSites.rows;
      const newTopSites = nextProps.TopSites && nextProps.TopSites.rows;
      if (
        prevTopSites &&
        prevTopSites[this.state.draggedIndex] &&
        prevTopSites[this.state.draggedIndex].url ===
          this.state.draggedSite.url &&
        (!newTopSites[this.state.draggedIndex] ||
          newTopSites[this.state.draggedIndex].url !==
            this.state.draggedSite.url)
      ) {
        // We got the new order from the redux store via props. We can clear state now.
        this.setState(TopSiteList.DEFAULT_STATE);
      }
    }
  }

  userEvent(event, index) {
    this.props.dispatch(
      ac.UserEvent({
        event,
        source: TOP_SITES_SOURCE,
        action_position: index,
      })
    );
  }

  onDragEvent(event, index, link, title) {
    switch (event.type) {
      case "dragstart":
        this.dropped = false;
        this.setState({
          draggedIndex: index,
          draggedSite: link,
          draggedTitle: title,
          activeIndex: null,
        });
        this.userEvent("DRAG", index);
        break;
      case "dragend":
        if (!this.dropped) {
          // If there was no drop event, reset the state to the default.
          this.setState(TopSiteList.DEFAULT_STATE);
        }
        break;
      case "dragenter":
        if (index === this.state.draggedIndex) {
          this.setState({ topSitesPreview: null });
        } else {
          this.setState({
            topSitesPreview: this._makeTopSitesPreview(index),
          });
        }
        break;
      case "drop":
        if (index !== this.state.draggedIndex) {
          this.dropped = true;
          this.props.dispatch(
            ac.AlsoToMain({
              type: at.TOP_SITES_INSERT,
              data: {
                site: {
                  url: this.state.draggedSite.url,
                  label: this.state.draggedTitle,
                  customScreenshotURL: this.state.draggedSite
                    .customScreenshotURL,
                  // Only if the search topsites experiment is enabled
                  ...(this.state.draggedSite.searchTopSite && {
                    searchTopSite: true,
                  }),
                },
                index,
                draggedFromIndex: this.state.draggedIndex,
              },
            })
          );
          this.userEvent("DROP", index);
        }
        break;
    }
  }

  _getTopSites() {
    // Make a copy of the sites to truncate or extend to desired length
    let topSites = this.props.TopSites.rows.slice();
    topSites.length = this.props.TopSitesRows * TOP_SITES_MAX_SITES_PER_ROW;
    return topSites;
  }

  /**
   * Make a preview of the topsites that will be the result of dropping the currently
   * dragged site at the specified index.
   */
  _makeTopSitesPreview(index) {
    const topSites = this._getTopSites();
    topSites[this.state.draggedIndex] = null;
    const preview = topSites.map(site =>
      site && (site.isPinned || site.sponsored_position) ? site : null
    );
    const unpinned = topSites.filter(
      site => site && !site.isPinned && !site.sponsored_position
    );
    const siteToInsert = Object.assign({}, this.state.draggedSite, {
      isPinned: true,
      isDragged: true,
    });

    if (!preview[index]) {
      preview[index] = siteToInsert;
    } else {
      // Find the hole to shift the pinned site(s) towards. We shift towards the
      // hole left by the site being dragged.
      let holeIndex = index;
      const indexStep = index > this.state.draggedIndex ? -1 : 1;
      while (preview[holeIndex]) {
        holeIndex += indexStep;
      }

      // Shift towards the hole.
      const shiftingStep = index > this.state.draggedIndex ? 1 : -1;
      while (
        index > this.state.draggedIndex ? holeIndex < index : holeIndex > index
      ) {
        let nextIndex = holeIndex + shiftingStep;
        while (preview[nextIndex] && preview[nextIndex].sponsored_position) {
          nextIndex += shiftingStep;
        }
        preview[holeIndex] = preview[nextIndex];
        holeIndex = nextIndex;
      }
      preview[index] = siteToInsert;
    }

    // Fill in the remaining holes with unpinned sites.
    for (let i = 0; i < preview.length; i++) {
      if (!preview[i]) {
        preview[i] = unpinned.shift() || null;
      }
    }

    return preview;
  }

  onActivate(index) {
    this.setState({ activeIndex: index });
  }

  render() {
    const { props } = this;
    const topSites = this.state.topSitesPreview || this._getTopSites();
    const topSitesUI = [];
    const commonProps = {
      onDragEvent: this.onDragEvent,
      dispatch: props.dispatch,
    };
    // We assign a key to each placeholder slot. We need it to be independent
    // of the slot index (i below) so that the keys used stay the same during
    // drag and drop reordering and the underlying DOM nodes are reused.
    // This mostly (only?) affects linux so be sure to test on linux before changing.
    let holeIndex = 0;

    // On narrow viewports, we only show 6 sites per row. We'll mark the rest as
    // .hide-for-narrow to hide in CSS via @media query.
    const maxNarrowVisibleIndex = props.TopSitesRows * 6;

    for (let i = 0, l = topSites.length; i < l; i++) {
      const link =
        topSites[i] &&
        Object.assign({}, topSites[i], {
          iconType: this.props.topSiteIconType(topSites[i]),
        });
      const slotProps = {
        key: link ? link.url : holeIndex++,
        index: i,
      };
      if (i >= maxNarrowVisibleIndex) {
        slotProps.className = "hide-for-narrow";
      }
      topSitesUI.push(
        !link ? (
          <TopSitePlaceholder {...slotProps} {...commonProps} />
        ) : (
          <TopSite
            link={link}
            activeIndex={this.state.activeIndex}
            onActivate={this.onActivate}
            {...slotProps}
            {...commonProps}
            colors={props.colors}
          />
        )
      );
    }
    return (
      <ul
        className={`top-sites-list${
          this.state.draggedSite ? " dnd-active" : ""
        }`}
      >
        {topSitesUI}
      </ul>
    );
  }
}
