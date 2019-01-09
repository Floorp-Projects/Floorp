import {mountWithIntl} from "test/unit/utils";
import React from "react";
import {ReturnToAMO} from "content-src/asrouter/templates/ReturnToAMO/ReturnToAMO";

describe("<ReturnToAMO>", () => {
  let dispatch;
  let onReady;
  beforeEach(() => {
    dispatch = sinon.stub();
    onReady = sinon.stub();
    const content = {
      primary_button: {},
      secondary_button: {},
    };

    mountWithIntl(<ReturnToAMO onReady={onReady} dispatch={dispatch} content={content} />);
  });

  it("should call onReady on componentDidMount", () => {
    assert.calledOnce(onReady);
  });
});
