import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {FormattedMessage} from "react-intl";
import React from "react";

class SelectableSearchShortcut extends React.PureComponent {
  render() {
    const {shortcut, selected} = this.props;
    const imageStyle = {backgroundImage: `url("${shortcut.tippyTopIcon}")`};
    return (
      <div className="top-site-outer">
        <input type="checkbox" id={shortcut.keyword} name={shortcut.keyword} checked={selected} onChange={this.props.onChange} />
        <label htmlFor={shortcut.keyword}>
          <div className="top-site-inner">
            <span>
              <div className="tile">
                <div className="top-site-icon rich-icon" style={imageStyle} data-fallback="@" />
                <div className="top-site-icon search-topsite" />
              </div>
              <div className="title">
                <span dir="auto">{shortcut.keyword}</span>
              </div>
            </span>
          </div>
        </label>
      </div>
    );
  }
}

export class SearchShortcutsForm extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleChange = this.handleChange.bind(this);
    this.onCancelButtonClick = this.onCancelButtonClick.bind(this);
    this.onSaveButtonClick = this.onSaveButtonClick.bind(this);

    // clone the shortcuts and add them to the state so we can add isSelected property
    const shortcuts = [];
    const {rows, searchShortcuts} = props.TopSites;
    searchShortcuts.forEach(shortcut => {
      shortcuts.push({
        ...shortcut,
        isSelected: !!rows.find(row => row && row.isPinned && row.searchTopSite && row.label === shortcut.keyword)
      });
    });
    this.state = {shortcuts};
  }

  handleChange(event) {
    const {target} = event;
    const {name, checked} = target;
    this.setState(prevState => {
      const shortcuts = prevState.shortcuts.slice();
      let shortcut = shortcuts.find(({keyword}) => keyword === name);
      shortcut.isSelected = checked;
      return {shortcuts};
    });
  }

  onCancelButtonClick(ev) {
    ev.preventDefault();
    this.props.onClose();
  }

  onSaveButtonClick(ev) {
    ev.preventDefault();

    // Check if there were any changes and act accordingly
    const {rows} = this.props.TopSites;
    const pinQueue = [];
    const unpinQueue = [];
    this.state.shortcuts.forEach(shortcut => {
      const alreadyPinned = rows.find(row => row && row.isPinned && row.searchTopSite && row.label === shortcut.keyword);
      if (shortcut.isSelected && !alreadyPinned) {
        pinQueue.push(this._searchTopSite(shortcut));
      } else if (!shortcut.isSelected && alreadyPinned) {
        unpinQueue.push({url: alreadyPinned.url});
      }
    });

    // Pin the pinQueue
    if (pinQueue.length > 0) {
      // First find the available slots. A slot is available if it isn't pinned
      // or if it's a pinned shortcut that we are about to unpin.
      const availableSlots = [];
      rows.forEach((row, index) => {
        if (!row || !row.isPinned || (row.searchTopSite && unpinQueue.find(site => row.url === site.url))) {
          availableSlots.push(index);
        }
      });

      pinQueue.forEach(shortcut => {
        this.props.dispatch(ac.OnlyToMain({
          type: at.TOP_SITES_PIN,
          data: {
            site: shortcut,
            index: availableSlots.shift()
          }
        }));
      });
    }

    // Unpin the unpinQueue.
    unpinQueue.forEach(shortcut => {
      this.props.dispatch(ac.OnlyToMain({
        type: at.TOP_SITES_UNPIN,
        data: {site: shortcut}
      }));
    });

    this.props.onClose();
  }

  _searchTopSite(shortcut) {
    return {
      url: shortcut.url,
      searchTopSite: true,
      label: shortcut.keyword
    };
  }

  render() {
    return (
      <form className="topsite-form">
        <div className="search-shortcuts-container">
          <h3 className="section-title">
            <FormattedMessage id="section_menu_action_add_search_engine" />
          </h3>
          <div>
            {this.state.shortcuts.map(shortcut => <SelectableSearchShortcut key={shortcut.keyword} shortcut={shortcut} selected={shortcut.isSelected} onChange={this.handleChange} />)}
          </div>
        </div>
        <section className="actions">
          <button className="cancel" type="button" onClick={this.onCancelButtonClick}>
            <FormattedMessage id="topsites_form_cancel_button" />
          </button>
          <button className="done" type="submit" onClick={this.onSaveButtonClick}>
            <FormattedMessage id="topsites_form_save_button" />
          </button>
        </section>
      </form>
    );
  }
}
