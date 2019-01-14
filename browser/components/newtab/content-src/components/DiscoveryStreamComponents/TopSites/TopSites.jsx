import {connect} from "react-redux";
import {TopSites as OldTopSites} from "content-src/components/TopSites/TopSites";
import React from "react";

export class _TopSites extends React.PureComponent {
  render() {
    return (
      <div className="ds-top-sites">
        <OldTopSites />
      </div>
    );
  }
}

export const TopSites = connect(state => ({TopSites: state.TopSites}))(_TopSites);
