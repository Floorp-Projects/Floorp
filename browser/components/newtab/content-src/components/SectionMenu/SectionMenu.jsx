import {actionCreators as ac} from "common/Actions.jsm";
import {ContextMenu} from "content-src/components/ContextMenu/ContextMenu";
import {injectIntl} from "react-intl";
import React from "react";
import {SectionMenuOptions} from "content-src/lib/section-menu-options";

const DEFAULT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "RemoveSection", "CheckCollapsed", "Separator", "ManageSection"];
const WEBEXT_SECTION_MENU_OPTIONS = ["MoveUp", "MoveDown", "Separator", "CheckCollapsed", "Separator", "ManageWebExtension"];

export class _SectionMenu extends React.PureComponent {
  getOptions() {
    const {props} = this;

    const propOptions = props.isWebExtension ? [...WEBEXT_SECTION_MENU_OPTIONS] : [...DEFAULT_SECTION_MENU_OPTIONS];
    // Remove the move related options if the section is fixed
    if (props.isFixed) {
      propOptions.splice(propOptions.indexOf("MoveUp"), 3);
    }
    // Prepend custom options and a separator
    if (props.extraOptions) {
      propOptions.splice(0, 0, ...props.extraOptions, "Separator");
    }
    // Insert privacy notice before the last option ("ManageSection")
    if (props.privacyNoticeURL) {
      propOptions.splice(-1, 0, "PrivacyNotice");
    }

    const options = propOptions.map(o => SectionMenuOptions[o](props)).map(option => {
      const {action, id, type, userEvent} = option;
      if (!type && id) {
        option.label = props.intl.formatMessage({id});
        option.onClick = () => {
          props.dispatch(action);
          if (userEvent) {
            props.dispatch(ac.UserEvent({
              event: userEvent,
              source: props.source,
            }));
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

export const SectionMenu = injectIntl(_SectionMenu);
