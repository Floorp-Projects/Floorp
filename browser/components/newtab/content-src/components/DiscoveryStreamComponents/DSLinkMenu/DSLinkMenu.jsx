import {FormattedMessage, injectIntl} from "react-intl";
import {LinkMenu} from "content-src/components/LinkMenu/LinkMenu";
import React from "react";

export class _DSLinkMenu extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      activeCard: null,
      showContextMenu: false,
    };
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
    this.setState({showContextMenu});
  }

  onMenuShow() {
    const dsLinkMenuHostDiv = this.contextMenuButtonRef.current.parentElement;
    if (window.scrollMaxX > 0) {
      dsLinkMenuHostDiv.parentElement.classList.add("last-item");
    }
    dsLinkMenuHostDiv.parentElement.classList.add("active");
  }

  render() {
    const {index, dispatch} = this.props;
    const isContextMenuOpen = this.state.showContextMenu && this.state.activeCard === index;
    const TOP_STORIES_CONTEXT_MENU_OPTIONS = ["CheckBookmarkOrArchive", "CheckSavedToPocket", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];
    const title = this.props.title || this.props.source;
    const type = this.props.type || "DISCOVERY_STREAM";

    return (<div>
      <button ref={this.contextMenuButtonRef}
              className="context-menu-button icon"
              title={this.props.intl.formatMessage({id: "context_menu_title"})}
              onClick={this.onMenuButtonClick}>
        <span className="sr-only">
          <FormattedMessage id="context_menu_button_sr" values={{title}} />
        </span>
      </button>
      {isContextMenuOpen &&
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
            bookmarkGuid: this.props.bookmarkGuid,
          }} />
      }
    </div>);
  }
}

export const DSLinkMenu = injectIntl(_DSLinkMenu);
