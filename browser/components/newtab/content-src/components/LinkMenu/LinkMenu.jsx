import {actionCreators as ac} from "common/Actions.jsm";
import {connect} from "react-redux";
import {ContextMenu} from "content-src/components/ContextMenu/ContextMenu";
import {injectIntl} from "react-intl";
import {LinkMenuOptions} from "content-src/lib/link-menu-options";
import React from "react";

const DEFAULT_SITE_MENU_OPTIONS = ["CheckPinTopSite", "EditTopSite", "Separator", "OpenInNewWindow", "OpenInPrivateWindow", "Separator", "BlockUrl"];

export class _LinkMenu extends React.PureComponent {
  getOptions() {
    const {props} = this;
    const {site, index, source, isPrivateBrowsingEnabled, siteInfo, platform} = props;

    // Handle special case of default site
    const propOptions = !site.isDefault ? props.options : DEFAULT_SITE_MENU_OPTIONS;

    const options = propOptions.map(o => LinkMenuOptions[o](site, index, source, isPrivateBrowsingEnabled, siteInfo, platform)).map(option => {
      const {action, impression, id, string_id, type, userEvent} = option;
      if (!type && id) {
        option.label = props.intl.formatMessage({id: string_id || id});
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            const userEventData = Object.assign({
              event: userEvent,
              source,
              action_position: index
            }, siteInfo);
            props.dispatch(ac.UserEvent(userEventData));
          }
          if (impression && props.shouldSendImpressionStats) {
            props.dispatch(impression);
          }
        };
      }
      return option;
    });

    // This is for accessibility to support making each item tabbable.
    // We want to know which item is the first and which item
    // is the last, so we can close the context menu accordingly.
    options[0].first = true;
    options[options.length - 1].last = true;
    return options;
  }

  render() {
    return (<ContextMenu
      onUpdate={this.props.onUpdate}
      options={this.getOptions()} />);
  }
}

const getState = state => ({isPrivateBrowsingEnabled: state.Prefs.values.isPrivateBrowsingEnabled, platform: state.Prefs.values.platform});
export const LinkMenu = connect(getState)(injectIntl(_LinkMenu));
