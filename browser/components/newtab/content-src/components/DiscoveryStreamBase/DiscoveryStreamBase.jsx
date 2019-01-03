import {connect} from "react-redux";
import React from "react";

export class _DiscoveryStreamBase extends React.PureComponent {
  render() {
    return (
      <div className="discovery-stream layout">
        {this.props.DiscoveryStream.layout.map((section, sectionIndex) => (
          <div key={`section-${sectionIndex}`} className={`column column-${section.width}`}>
            {section.components.map((component, componentIndex) => (
              <div key={`component-${componentIndex}`}>
                <div>{component.type}</div>
              </div>
            ))}
          </div>
        ))}
      </div>
    );
  }
}

export const DiscoveryStreamBase = connect(state => ({DiscoveryStream: state.DiscoveryStream}))(_DiscoveryStreamBase);
