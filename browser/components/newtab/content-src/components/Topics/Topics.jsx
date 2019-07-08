import React from "react";

export class Topic extends React.PureComponent {
  render() {
    const { url, name } = this.props;
    return (
      <li>
        <a key={name} href={url}>
          {name}
        </a>
      </li>
    );
  }
}

export class Topics extends React.PureComponent {
  render() {
    const { topics } = this.props;
    return (
      <span className="topics">
        <span data-l10n-id="newtab-pocket-read-more" />
        <ul>
          {topics &&
            topics.map(t => <Topic key={t.name} url={t.url} name={t.name} />)}
        </ul>
      </span>
    );
  }
}
