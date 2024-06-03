/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect, useRef, useState } from "react";
import { useDispatch, useSelector } from "react-redux";
import { actionCreators as ac, actionTypes as at } from "common/Actions.mjs";

function LocationSearch({ outerClassName }) {
  // should be the location object from suggestedLocations
  const [selectedLocation, setSelectedLocation] = useState("");
  const suggestedLocations = useSelector(
    state => state.Weather.suggestedLocations
  );
  const locationSearchString = useSelector(
    state => state.Weather.locationSearchString
  );
  const [userInput, setUserInput] = useState(locationSearchString || "");
  const inputRef = useRef(null);

  const dispatch = useDispatch();

  useEffect(() => {
    if (selectedLocation) {
      dispatch(
        ac.AlsoToMain({
          type: at.WEATHER_LOCATION_DATA_UPDATE,
          data: {
            city: selectedLocation.localized_name,
            adminName: selectedLocation.administrative_area,
            country: selectedLocation.country,
          },
        })
      );
      dispatch(ac.SetPref("weather.query", selectedLocation.key));
      dispatch(
        ac.BroadcastToContent({
          type: at.WEATHER_SEARCH_ACTIVE,
          data: false,
        })
      );
    }
  }, [selectedLocation, dispatch]);

  // when component mounts, set focus to input
  useEffect(() => {
    inputRef?.current?.focus();
  }, [inputRef]);

  function handleChange(event) {
    const { value } = event.target;
    setUserInput(value);
    // if the user input contains less than three characters and suggestedLocations is not an empty array,
    // reset suggestedLocations to [] so there arent incorrect items in the datalist
    if (value.length < 3 && suggestedLocations.length) {
      dispatch(
        ac.AlsoToMain({
          type: at.WEATHER_LOCATION_SUGGESTIONS_UPDATE,
          data: [],
        })
      );
    }
    // find match in suggestedLocation array
    const match = suggestedLocations?.find(({ key }) => key === value);
    if (match) {
      setSelectedLocation(match);
      setUserInput(
        `${match.localized_name}, ${match.administrative_area.localized_name}`
      );
    } else if (value.length >= 3 && !match) {
      dispatch(
        ac.AlsoToMain({
          type: at.WEATHER_LOCATION_SEARCH_UPDATE,
          data: value,
        })
      );
    }
  }

  function handleCloseSearch() {
    dispatch(
      ac.BroadcastToContent({
        type: at.WEATHER_SEARCH_ACTIVE,
        data: false,
      })
    );
    setUserInput("");
  }

  function handleKeyDown(e) {
    if (e.key === "Escape") {
      handleCloseSearch();
    }
  }

  return (
    <div className={`${outerClassName} location-search`}>
      <div className="location-input-wrapper">
        <div className="search-icon" />
        <input
          ref={inputRef}
          list="merino-location-list"
          type="text"
          placeholder="Search location"
          onChange={handleChange}
          value={userInput}
          onKeyDown={handleKeyDown}
        />
        <button className="close-icon" onClick={handleCloseSearch} />
        <datalist id="merino-location-list">
          {(suggestedLocations || []).map(location => (
            <option value={location.key} key={location.key}>
              {location.localized_name},{" "}
              {location.administrative_area.localized_name}
            </option>
          ))}
        </datalist>
      </div>
    </div>
  );
}

export { LocationSearch };
