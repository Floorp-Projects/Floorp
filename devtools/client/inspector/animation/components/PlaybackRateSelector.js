/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { getFormatStr } = require("../utils/l10n");

const PLAYBACK_RATES = [0.1, 0.25, 0.5, 1, 2, 5, 10];

class PlaybackRateSelector extends PureComponent {
  static get propTypes() {
    return {
      animations: PropTypes.arrayOf(PropTypes.object).isRequired,
      playbackRates: PropTypes.arrayOf(PropTypes.number).isRequired,
      setAnimationsPlaybackRate: PropTypes.func.isRequired,
    };
  }

  static getDerivedStateFromProps(props, state) {
    const { animations, playbackRates } = props;

    const currentPlaybackRates = sortAndUnique(
      animations.map(a => a.state.playbackRate)
    );
    const options = sortAndUnique([
      ...PLAYBACK_RATES,
      ...playbackRates,
      ...currentPlaybackRates,
    ]);

    if (currentPlaybackRates.length === 1) {
      return {
        options,
        selected: currentPlaybackRates[0],
      };
    }

    // When the animations displayed have mixed playback rates, we can't
    // select any of the predefined ones.
    return {
      options: ["", ...options],
      selected: "",
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      options: [],
      selected: 1,
    };
  }

  onChange(e) {
    const { setAnimationsPlaybackRate } = this.props;

    if (!e.target.value) {
      return;
    }

    setAnimationsPlaybackRate(e.target.value);
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

const mapStateToProps = state => {
  return {
    playbackRates: state.animations.playbackRates,
  };
};

module.exports = connect(mapStateToProps)(PlaybackRateSelector);
