import React from "react";

export class DSEmptyState extends React.PureComponent {
  render() {
    return (
      <div className="section-empty-state">
        <div className="empty-state-message">
          <h2>You are caught up!</h2>
          <p>Check back later for more stories.</p>
        </div>
      </div>
    );
  }
}
