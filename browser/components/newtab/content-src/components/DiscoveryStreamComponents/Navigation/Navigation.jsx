import React from "react";

export class Topic extends React.PureComponent {
  render() {
    const {url, name} = this.props;
    return (<li><a key={name} href={url}>{name}</a></li>);
  }
}

export class Navigation extends React.PureComponent {
  render() {
    const {links} = this.props || [];
    const {alignment} = this.props || "centered";
    return (
      <span className={`ds-navigation ds-navigation-${alignment}`}>
        <ul>
          {links && links.map(t => <Topic key={t.name} url={t.url} name={t.name} />)}
        </ul>
      </span>
    );
  }
}
