import {connect} from "react-redux";
import React from "react";

export class _TopSites extends React.PureComponent {
  render() {
    return (
      <div className="ds-topsites">
        Top Sites
      </div>
    );
  }
}

export const TopSites = connect(state => ({TopSites: state.TopSites}))(_TopSites);
