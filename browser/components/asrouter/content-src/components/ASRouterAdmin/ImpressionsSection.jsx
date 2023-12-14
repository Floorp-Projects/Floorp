/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { ASRouterUtils } from "newtab/content-src/asrouter/asrouter-utils";
import React, {
  useState,
  useMemo,
  useCallback,
  useEffect,
  useRef,
} from "react";

const stringify = json => JSON.stringify(json, null, 2);

export const ImpressionsSection = ({
  messageImpressions,
  groupImpressions,
  screenImpressions,
}) => {
  const handleSaveMessageImpressions = useCallback(newImpressions => {
    ASRouterUtils.editState("messageImpressions", newImpressions);
  }, []);
  const handleSaveGroupImpressions = useCallback(newImpressions => {
    ASRouterUtils.editState("groupImpressions", newImpressions);
  }, []);
  const handleSaveScreenImpressions = useCallback(newImpressions => {
    ASRouterUtils.editState("screenImpressions", newImpressions);
  }, []);

  const handleResetMessageImpressions = useCallback(() => {
    ASRouterUtils.sendMessage({ type: "RESET_MESSAGE_STATE" });
  }, []);
  const handleResetGroupImpressions = useCallback(() => {
    ASRouterUtils.sendMessage({ type: "RESET_GROUPS_STATE" });
  }, []);
  const handleResetScreenImpressions = useCallback(() => {
    ASRouterUtils.sendMessage({ type: "RESET_SCREEN_IMPRESSIONS" });
  }, []);

  return (
    <div className="impressions-section">
      <ImpressionsItem
        impressions={messageImpressions}
        label="Message Impressions"
        description="Message impressions are stored in an object, where each key is a message ID and each value is an array of timestamps. They are cleaned up when a message with that ID stops existing in ASRouter state (such as at the end of an experiment)."
        onSave={handleSaveMessageImpressions}
        onReset={handleResetMessageImpressions}
      />
      <ImpressionsItem
        impressions={groupImpressions}
        label="Group Impressions"
        description="Group impressions are stored in an object, where each key is a group ID and each value is an array of timestamps. They are never cleaned up."
        onSave={handleSaveGroupImpressions}
        onReset={handleResetGroupImpressions}
      />
      <ImpressionsItem
        impressions={screenImpressions}
        label="Screen Impressions"
        description="Screen impressions are stored in an object, where each key is a screen ID and each value is the most recent timestamp that screen was shown. They are never cleaned up."
        onSave={handleSaveScreenImpressions}
        onReset={handleResetScreenImpressions}
      />
    </div>
  );
};

const ImpressionsItem = ({
  impressions,
  label,
  description,
  validator,
  onSave,
  onReset,
}) => {
  const [json, setJson] = useState(stringify(impressions));

  const modified = useRef(false);

  const isValidJson = useCallback(
    text => {
      try {
        JSON.parse(text);
        return validator ? validator(text) : true;
      } catch (e) {
        return false;
      }
    },
    [validator]
  );

  const jsonIsInvalid = useMemo(() => !isValidJson(json), [json, isValidJson]);

  const handleChange = useCallback(e => {
    setJson(e.target.value);
    modified.current = true;
  }, []);
  const handleSave = useCallback(() => {
    if (jsonIsInvalid) {
      return;
    }
    const newImpressions = JSON.parse(json);
    modified.current = false;
    onSave(newImpressions);
  }, [json, jsonIsInvalid, onSave]);
  const handleReset = useCallback(() => {
    modified.current = false;
    onReset();
  }, [onReset]);

  useEffect(() => {
    if (!modified.current) {
      setJson(stringify(impressions));
    }
  }, [impressions]);

  return (
    <div className="impressions-item">
      <span className="impressions-category">{label}</span>
      {description ? (
        <p className="impressions-description">{description}</p>
      ) : null}
      <div className="impressions-inner-box">
        <div className="impressions-buttons">
          <button
            className="button primary"
            disabled={jsonIsInvalid}
            onClick={handleSave}
          >
            Save
          </button>
          <button className="button reset" onClick={handleReset}>
            Reset
          </button>
        </div>
        <div className="impressions-editor">
          <textarea
            className="general-textarea"
            value={json}
            onChange={handleChange}
          />
        </div>
      </div>
    </div>
  );
};
