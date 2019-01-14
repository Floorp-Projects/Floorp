import React from "react";

export class Topic extends React.PureComponent {
  render() {
    const {url, name} = this.props;
    return (<li><a key={name} href={url}>{name}</a></li>);
  }
}

export class SectionTitle extends React.PureComponent {
  render() {
    const {topics} = this.props;
    return (
      <span className="ds-section-title">
        <ul>
          {topics && topics.map(t => <Topic key={t.name} url={t.url} name={t.name} />)}
          <li><a className="ds-more-recommendations">More Recommendations</a></li>
        </ul>
      </span>
    );
  }
}
