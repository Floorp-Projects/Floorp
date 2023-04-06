/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { addons } from "@storybook/addons";
// eslint-disable-next-line no-unused-vars
import { AddonPanel } from "@storybook/components";
import { FLUENT_CHANGED, FLUENT_SET_STRINGS } from "./constants.mjs";

export class FluentPanel extends React.Component {
  constructor(props) {
    super(props);
    this.channel = addons.getChannel();
    this.state = {
      name: null,
      strings: [],
    };
  }

  componentDidMount() {
    const { api } = this.props;
    api.on(FLUENT_CHANGED, this.handleFluentChanged);
  }

  componentWillUnmount() {
    const { api } = this.props;
    api.off(FLUENT_CHANGED, this.handleFluentChanged);
  }

  handleFluentChanged = strings => {
    let storyData = this.props.api.getCurrentStoryData();
    let fileName = `${storyData.component}.ftl`;
    this.setState(state => ({ ...state, strings, fileName }));
  };

  onInput = e => {
    this.setState(state => {
      let strings = [];
      for (let [key, value] of state.strings) {
        if (key == e.target.name) {
          strings.push([key, e.target.value]);
        } else {
          strings.push([key, value]);
        }
      }
      let stringified = strings
        .map(([key, value]) => `${key} = ${value}`)
        .join("\n");
      this.channel.emit(FLUENT_SET_STRINGS, stringified);
      const { fluentStrings } = this.props.api.getGlobals();
      this.props.api.updateGlobals({
        fluentStrings: { ...fluentStrings, [state.fileName]: strings },
      });
      return { ...state, strings };
    });
  };

  render() {
    const { api, active } = this.props;
    const { strings } = this.state;
    return (
      <AddonPanel active={!!active} api={api}>
        {strings.map(([identifier, value]) => (
          <div key={identifier}>
            <label>
              {identifier} =
              <textarea
                name={identifier}
                onInput={this.onInput}
                defaultValue={value}
              ></textarea>
            </label>
          </div>
        ))}
      </AddonPanel>
    );
  }
}
