import {connect} from "react-redux";
import {TopSites as OldTopSites} from "content-src/components/TopSites/TopSites";
import React from "react";

export class _TopSites extends React.PureComponent {
  render() {
    const header = this.props.header || {};
    return (
      <div className="ds-top-sites">
        {header.title ? (
          <div className="ds-header">
            <span className="icon icon-small-spacer icon-topsites" />
            <span className="ds-header-title">{header.title}</span>
          </div>
        ) : null}
        <OldTopSites />
      </div>
    );
  }
}

export const TopSites = connect(state => ({TopSites: state.TopSites}))(_TopSites);
