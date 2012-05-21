/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var BUGNUMBER = 329257;
var summary = "namespace prefix in E4X dot query";

printBugNumber(BUGNUMBER);
START(summary);

var ns12 = new Namespace("foo");
var nestInfo = <a xmlns="foo">
	<ParameterAvailabilityInfo>
		<ParameterID>10</ParameterID>
		<PowerOfTen>1</PowerOfTen>
	</ParameterAvailabilityInfo>
	<ParameterAvailabilityInfo>
		<ParameterID>100</ParameterID>
		<PowerOfTen>2</PowerOfTen>
	</ParameterAvailabilityInfo>
	<ParameterAvailabilityInfo>
		<ParameterID>1000</ParameterID>
		<PowerOfTen>3</PowerOfTen>
	</ParameterAvailabilityInfo>
</a>;

var paramInfo = nestInfo.ns12::ParameterAvailabilityInfo;
var paramInfo100 = nestInfo.ns12::ParameterAvailabilityInfo.(ns12::ParameterID == 100);
TEST(1, 2, Number(paramInfo100.ns12::PowerOfTen));

default xml namespace = ns12;
var paramInfo100 = nestInfo.ParameterAvailabilityInfo.(ParameterID == 100);
TEST(2, 2, Number(paramInfo100.ns12::PowerOfTen));
