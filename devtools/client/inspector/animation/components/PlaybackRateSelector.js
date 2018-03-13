/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getFormatStr } = require("../utils/l10n");

const PLAYBACK_RATES = [.1, .25, .5, 1, 2, 5, 10];

class PlaybackRateSelector extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      setAnimationsPlaybackRate: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      options: [],
      selected: 1,
    };
  }

  componentWillMount() {
    this.updateState(this.props);
  }

  componentWillReceiveProps(nextProps) {
    this.updateState(nextProps);
  }

  getPlaybackRates(animations) {
    return sortAndUnique(animations.map(a => a.state.playbackRate));
  }

  getSelectablePlaybackRates(animationsRates) {
    return sortAndUnique(PLAYBACK_RATES.concat(animationsRates));
  }

  onChange(e) {
    const { setAnimationsPlaybackRate } = this.props;

    if (!e.target.value) {
      return;
    }

    setAnimationsPlaybackRate(e.target.value);
  }

  updateState(props) {
    const { animations } = props;

    let options;
    let selected;
    const rates = this.getPlaybackRates(animations);

    if (rates.length === 1) {
      options = this.getSelectablePlaybackRates(rates);
      selected = rates[0];
    } else {
      // When the animations displayed have mixed playback rates, we can't
      // select any of the predefined ones.
      options = ["", ...PLAYBACK_RATES];
      selected = "";
    }

    this.setState({ options, selected });
  }

  render() {
    const { options, selected } = this.state;

    return dom.select(
      {
        className: "playback-rate-selector devtools-button",
        onChange: this.onChange.bind(this),
      },
      options.map(rate => {
        return dom.option(
          {
            selected: rate === selected ? "true" : null,
            value: rate,
          },
          rate ? getFormatStr("player.playbackRateLabel", rate) : "-"
        );
      })
    );
  }
}

function sortAndUnique(array) {
  return [...new Set(array)].sort((a, b) => a > b);
}

module.exports = PlaybackRateSelector;
