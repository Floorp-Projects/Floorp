import React, { Component } from 'react';
import I from "immutable"

class App extends Component {
  componentDidMount() {
    this.fields = new I.Map({a:2})
  }

  onClick = () => {
    const f = this.fields
    console.log(f)
  }

  render() {
    return (
      <div className="App">
        <button onClick={this.onClick}>Click Me</button>
      </div>
    );
  }
}

window.clickButton = () => document.querySelector("button").click()

export default App;
