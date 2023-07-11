/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { addons } from "@storybook/addons";
// eslint-disable-next-line no-unused-vars
import { AddonPanel } from "@storybook/components";
import { FLUENT_CHANGED, FLUENT_SET_STRINGS } from "./constants.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "./fluent-panel.css";

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
          let stringValue = e.target.value;
          if (stringValue.startsWith(".")) {
            stringValue = "\n" + stringValue;
          }
          strings.push([key, stringValue]);
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
    if (strings.length === 0) {
      return (
        <AddonPanel active={!!active} api={api}>
          <div className="addon-panel-body">
            <div className="addon-panel-message">
              This story is not configured to use Fluent.
            </div>
          </div>
        </AddonPanel>
      );
    }

    return (
      <AddonPanel active={!!active} api={api}>
        <div className="addon-panel-body">
          <table aria-hidden="false" className="addon-panel-table">
            <thead className="addon-panel-table-head">
              <tr>
                <th>
                  <span>Identifier</span>
                </th>
                <th>
                  <span>String</span>
                </th>
              </tr>
            </thead>
            <tbody className="addon-panel-table-body">
              {strings.map(([identifier, value]) => (
                <tr key={identifier}>
                  <td>
                    <span>{identifier}</span>
                  </td>
                  <td>
                    <label>
                      <textarea
                        name={identifier}
                        onInput={this.onInput}
                        defaultValue={value
                          .trim()
                          .split("\n")
                          .map(s => s.trim())
                          .join("\n")}
                      ></textarea>
                    </label>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </AddonPanel>
    );
  }
}
