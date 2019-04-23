import React from "react";

export class SimpleHashRouter extends React.PureComponent {
  constructor(props) {
    super(props);
    this.onHashChange = this.onHashChange.bind(this);
    this.state = {hash: global.location.hash};
  }

  onHashChange() {
    this.setState({hash: global.location.hash});
  }

  componentWillMount() {
    global.addEventListener("hashchange", this.onHashChange);
  }

  componentWillUnmount() {
    global.removeEventListener("hashchange", this.onHashChange);
  }

  render() {
    const [, ...routes] = this.state.hash.split("-");
    return React.cloneElement(this.props.children, {
      location: {
        hash: this.state.hash,
        routes,
      },
    });
  }
}
