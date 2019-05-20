/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const PropTypes = require("prop-types");

const { wrapRender } = require("../reps/rep-utils");
const { MODE } = require("../reps/constants");
const { ModePropType } = require("../reps/array");

const dom = require("react-dom-factories");
const { span } = dom;

GripLengthBubble.propTypes = {
  object: PropTypes.object.isRequired,
  maxLengthMap: PropTypes.instanceOf(Map).isRequired,
  getLength: PropTypes.func.isRequired,
  mode: ModePropType,
  visibilityThreshold: PropTypes.number,
};

function GripLengthBubble(props) {
  const {
    object,
    mode = MODE.SHORT,
    visibilityThreshold = 2,
    maxLengthMap,
    getLength,
    showZeroLength = false,
  } = props;

  const length = getLength(object);
  const isEmpty = length === 0;
  const isObvious =
    [MODE.SHORT, MODE.LONG].includes(mode) &&
    length > 0 &&
    length <= maxLengthMap.get(mode) &&
    length <= visibilityThreshold;
  if ((isEmpty && !showZeroLength) || isObvious) {
    return "";
  }

  return span(
    {
      className: "objectLengthBubble",
    },
    `(${length})`
  );
}

module.exports = {
  lengthBubble: wrapRender(GripLengthBubble),
};
