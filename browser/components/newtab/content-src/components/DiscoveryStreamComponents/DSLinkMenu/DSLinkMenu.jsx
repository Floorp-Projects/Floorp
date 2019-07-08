import { LinkMenu } from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

export class DSLinkMenu extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      activeCard: null,
      showContextMenu: false,
    };
    this.windowObj = this.props.windowObj || window; // Added to support unit tests
    this.onMenuButtonClick = this.onMenuButtonClick.bind(this);
    this.onMenuUpdate = this.onMenuUpdate.bind(this);
    this.onMenuShow = this.onMenuShow.bind(this);
    this.contextMenuButtonRef = React.createRef();
  }

  onMenuButtonClick(event) {
    event.preventDefault();
    this.setState({
      activeCard: this.props.index,
      showContextMenu: true,
    });
  }

  onMenuUpdate(showContextMenu) {
    if (!showContextMenu) {
      const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
      dsLinkMenuHostDiv.parentElement.classList.remove("active", "last-item");
    }
    this.setState({ showContextMenu });
  }

  nextAnimationFrame() {
    return new Promise(resolve => requestAnimationFrame(resolve));
  }

  async onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
    // Wait for next frame before computing scrollMaxX to allow fluent menu strings to be visible
    await this.nextAnimationFrame();
    if (this.windowObj.scrollMaxX > 0) {
      dsLinkMenuHostDiv.parentElement.classList.add("last-item");
    }
    dsLinkMenuHostDiv.parentElement.classList.add("active");
  }

  render() {
    const { index, dispatch } = this.props;
    const isContextMenuOpen =
      this.state.showContextMenu && this.state.activeCard === index;
    const TOP_STORIES_CONTEXT_MENU_OPTIONS = [
      "CheckBookmarkOrArchive",
      "CheckSavedToPocket",
      "Separator",
      "OpenInNewWindow",
      "OpenInPrivateWindow",
      "Separator",
      "BlockUrl",
    ];
    const type = this.props.type || "DISCOVERY_STREAM";
    const title = this.props.title || this.props.source;

    return (
      <div>
        <button
          ref={this.contextMenuButtonRef}
          aria-haspopup="true"
          className="context-menu-button icon"
          data-l10n-id="newtab-menu-content-tooltip"
          data-l10n-args={JSON.stringify({ title })}
          onClick={this.onMenuButtonClick}
        />
        {isContextMenuOpen && (
          <LinkMenu
            dispatch={dispatch}
            index={index}
            source={type.toUpperCase()}
            onUpdate={this.onMenuUpdate}
            onShow={this.onMenuShow}
            options={TOP_STORIES_CONTEXT_MENU_OPTIONS}
            shouldSendImpressionStats={true}
            site={{
              referrer: "https://getpocket.com/recommendations",
              title: this.props.title,
              type: this.props.type,
              url: this.props.url,
              guid: this.props.id,
              pocket_id: this.props.pocket_id,
              shim: this.props.shim,
              bookmarkGuid: this.props.bookmarkGuid,
            }}
          />
        )}
      </div>
    );
  }
}
