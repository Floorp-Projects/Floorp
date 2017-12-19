/*
 * Test for form auto fill content helper fill all inputs function.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const DEFAULT_ADDRESS_RECORD = {
  "guid": "123",
  "street-address": "2 Harrison St\nline2\nline3",
  "address-line1": "2 Harrison St",
  "address-line2": "line2",
  "address-line3": "line3",
  "address-level1": "CA",
  "country": "US",
  "tel": "+19876543210",
  "tel-national": "9876543210",
};

const ADDRESS_RECORD_2 = {
  "guid": "address2",
  "given-name": "John",
  "additional-name": "Middle",
  "family-name": "Doe",
  "postal-code": "940012345",
};

const DEFAULT_CREDITCARD_RECORD = {
  "guid": "123",
  "cc-exp-month": 1,
  "cc-exp-year": 2025,
  "cc-exp": "2025-01",
};

const TESTCASES = [
  {
    description: "Address form with street-address",
    document: `<form>
               <input autocomplete="given-name">
               <input autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with street-address, address-line[1, 2, 3]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               <input id="line3" autocomplete="address-line3">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with street-address, address-line1",
    document: `<form>
               <input autocomplete="given-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St line2 line3",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with street-address, address-line[1, 2]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with street-address, address-line[1, 3]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line3" autocomplete="address-line3">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with exact matching options in select",
    document: `<form>
               <input autocomplete="given-name">
               <select autocomplete="address-level1">
                 <option id="option-address-level1-XX" value="XX">Dummy</option>
                 <option id="option-address-level1-CA" value="CA">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-XX" value="XX">Dummy</option>
                 <option id="option-country-US" value="US">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-CA",
      "country": "option-country-US",
    }],
  },
  {
    description: "Address form with inexact matching options in select",
    document: `<form>
               <input autocomplete="given-name">
               <select autocomplete="address-level1">
                 <option id="option-address-level1-XX" value="XX">Dummy</option>
                 <option id="option-address-level1-OO" value="OO">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-XX" value="XX">Dummy</option>
                 <option id="option-country-OO" value="OO">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-OO",
      "country": "option-country-OO",
    }],
  },
  {
    description: "Address form with value-omitted options in select",
    document: `<form>
               <input autocomplete="given-name">
               <select autocomplete="address-level1">
                 <option id="option-address-level1-1" value="">Dummy</option>
                 <option id="option-address-level1-2" value="">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-1" value="">Dummy</option>
                 <option id="option-country-2" value="">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-2",
      "country": "option-country-2",
    }],
  },
  {
    description: "Address form with options with the same value in select ",
    document: `<form>
               <input autocomplete="given-name">
               <select autocomplete="address-level1">
                 <option id="option-address-level1-same1" value="same">Dummy</option>
                 <option id="option-address-level1-same2" value="same">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-same1" value="sametoo">Dummy</option>
                 <option id="option-country-same2" value="sametoo">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-same2",
      "country": "option-country-same2",
    }],
  },
  {
    description: "Address form without matching options in select for address-level1 and country",
    document: `<form>
               <input autocomplete="given-name">
               <select autocomplete="address-level1">
                 <option id="option-address-level1-dummy1" value="">Dummy</option>
                 <option id="option-address-level1-dummy2" value="">Dummy 2</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-dummy1" value="">Dummy</option>
                 <option id="option-country-dummy2" value="">Dummy 2</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Change the tel value of a profile to tel-national for a field without pattern and maxlength.",
    document: `<form>
               <input id="telephone">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Do not change the profile for an autocomplete=\"tel\" field without patern and maxlength.",
    document: `<form>
               <input id="tel" autocomplete="tel">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "autocomplete=\"tel\" field with `maxlength` can be filled with `tel` value.",
    document: `<form>
               <input id="telephone" autocomplete="tel" maxlength="12">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Still fill `tel-national` in a `tel` field with `maxlength` can be filled with `tel` value.",
    document: `<form>
               <input id="telephone" maxlength="12">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "`tel` field with `maxlength` can be filled with `tel-national` value.",
    document: `<form>
               <input id="telephone" maxlength="10">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "`tel` field with `pattern` attr can be filled with `tel` value.",
    document: `<form>
               <input id="telephone" pattern="[+][0-9]+">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "+19876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Change the tel value of a profile to tel-national one when the pattern is matched.",
    document: `<form>
               <input id="telephone" pattern="\d*">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Matching pattern when a field is with autocomplete=\"tel\".",
    document: `<form>
               <input id="tel" autocomplete="tel" pattern="[0-9]+">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Checking maxlength of tel field first when a field is with maxlength.",
    document: `<form>
               <input id="tel" autocomplete="tel" maxlength="10">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_ADDRESS_RECORD)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
      "tel": "9876543210",
      "tel-national": "9876543210",
    }],
  },
  {
    description: "Address form with maxlength restriction",
    document: `<form>
               <input autocomplete="given-name" maxlength="1">
               <input autocomplete="additional-name" maxlength="1">
               <input autocomplete="family-name" maxlength="1">
               <input autocomplete="postal-code" maxlength="5">
               </form>`,
    profileData: [Object.assign({}, ADDRESS_RECORD_2)],
    expectedResult: [{
      "guid": "address2",
      "given-name": "J",
      "additional-name": "M",
      "family-name": "D",
      "postal-code": "94001",
    }],
  },
  {
    description: "Address form with the special cases of the maxlength restriction",
    document: `<form>
               <input autocomplete="given-name" maxlength="-1">
               <input autocomplete="additional-name" maxlength="0">
               <input autocomplete="family-name" maxlength="1">
               </form>`,
    profileData: [Object.assign({}, ADDRESS_RECORD_2)],
    expectedResult: [{
      "guid": "address2",
      "given-name": "John",
      "family-name": "D",
      "postal-code": "940012345",
    }],
  },
  {
    description: "Credit Card form with matching options of cc-exp-year and cc-exp-month",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp-month">
                 <option id="option-cc-exp-month-01" value="1">01</option>
                 <option id="option-cc-exp-month-02" value="2">02</option>
                 <option id="option-cc-exp-month-03" value="3">03</option>
                 <option id="option-cc-exp-month-04" value="4">04</option>
                 <option id="option-cc-exp-month-05" value="5">05</option>
                 <option id="option-cc-exp-month-06" value="6">06</option>
                 <option id="option-cc-exp-month-07" value="7">07</option>
                 <option id="option-cc-exp-month-08" value="8">08</option>
                 <option id="option-cc-exp-month-09" value="9">09</option>
                 <option id="option-cc-exp-month-10" value="10">10</option>
                 <option id="option-cc-exp-month-11" value="11">11</option>
                 <option id="option-cc-exp-month-12" value="12">12</option>
               </select>
               <select autocomplete="cc-exp-year">
                 <option id="option-cc-exp-year-17" value="2017">17</option>
                 <option id="option-cc-exp-year-18" value="2018">18</option>
                 <option id="option-cc-exp-year-19" value="2019">19</option>
                 <option id="option-cc-exp-year-20" value="2020">20</option>
                 <option id="option-cc-exp-year-21" value="2021">21</option>
                 <option id="option-cc-exp-year-22" value="2022">22</option>
                 <option id="option-cc-exp-year-23" value="2023">23</option>
                 <option id="option-cc-exp-year-24" value="2024">24</option>
                 <option id="option-cc-exp-year-25" value="2025">25</option>
                 <option id="option-cc-exp-year-26" value="2026">26</option>
                 <option id="option-cc-exp-year-27" value="2027">27</option>
                 <option id="option-cc-exp-year-28" value="2028">28</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{
      "cc-exp-month": "option-cc-exp-month-01",
      "cc-exp-year": "option-cc-exp-year-25",
    }],
  },
  {
    description: "Credit Card form with matching options which contain labels",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp-month">
                 <option value="" selected="selected">Month</option>
                 <option label="01 - January" id="option-cc-exp-month-01" value="object:17">dummy</option>
                 <option label="02 - February" id="option-cc-exp-month-02" value="object:18">dummy</option>
                 <option label="03 - March" id="option-cc-exp-month-03" value="object:19">dummy</option>
                 <option label="04 - April" id="option-cc-exp-month-04" value="object:20">dummy</option>
                 <option label="05 - May" id="option-cc-exp-month-05" value="object:21">dummy</option>
                 <option label="06 - June" id="option-cc-exp-month-06" value="object:22">dummy</option>
                 <option label="07 - July" id="option-cc-exp-month-07" value="object:23">dummy</option>
                 <option label="08 - August" id="option-cc-exp-month-08" value="object:24">dummy</option>
                 <option label="09 - September" id="option-cc-exp-month-09" value="object:25">dummy</option>
                 <option label="10 - October" id="option-cc-exp-month-10" value="object:26">dummy</option>
                 <option label="11 - November" id="option-cc-exp-month-11" value="object:27">dummy</option>
                 <option label="12 - December" id="option-cc-exp-month-12" value="object:28">dummy</option>
               </select>
               <select autocomplete="cc-exp-year">
                 <option value="" selected="selected">Year</option>
                 <option label="2017" id="option-cc-exp-year-17" value="object:29">dummy</option>
                 <option label="2018" id="option-cc-exp-year-18" value="object:30">dummy</option>
                 <option label="2019" id="option-cc-exp-year-19" value="object:31">dummy</option>
                 <option label="2020" id="option-cc-exp-year-20" value="object:32">dummy</option>
                 <option label="2021" id="option-cc-exp-year-21" value="object:33">dummy</option>
                 <option label="2022" id="option-cc-exp-year-22" value="object:34">dummy</option>
                 <option label="2023" id="option-cc-exp-year-23" value="object:35">dummy</option>
                 <option label="2024" id="option-cc-exp-year-24" value="object:36">dummy</option>
                 <option label="2025" id="option-cc-exp-year-25" value="object:37">dummy</option>
                 <option label="2026" id="option-cc-exp-year-26" value="object:38">dummy</option>
                 <option label="2027" id="option-cc-exp-year-27" value="object:39">dummy</option>
                 <option label="2028" id="option-cc-exp-year-28" value="object:40">dummy</option>
                 <option label="2029" id="option-cc-exp-year-29" value="object:41">dummy</option>
                 <option label="2030" id="option-cc-exp-year-30" value="object:42">dummy</option>
                 <option label="2031" id="option-cc-exp-year-31" value="object:43">dummy</option>
                 <option label="2032" id="option-cc-exp-year-32" value="object:44">dummy</option>
                 <option label="2033" id="option-cc-exp-year-33" value="object:45">dummy</option>
                 <option label="2034" id="option-cc-exp-year-34" value="object:46">dummy</option>
                 <option label="2035" id="option-cc-exp-year-35" value="object:47">dummy</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{
      "cc-exp-month": "option-cc-exp-month-01",
      "cc-exp-year": "option-cc-exp-year-25",
    }],
  },
  {
    description: "Compound cc-exp: {MON1}/{YEAR2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="3/17">3/17</option>
                 <option value="1/25" id="selected-cc-exp">1/25</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON1}/{YEAR4}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="3/2017">3/2017</option>
                 <option value="1/2025" id="selected-cc-exp">1/2025</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON2}/{YEAR2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03/17">03/17</option>
                 <option value="01/25" id="selected-cc-exp">01/25</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON2}/{YEAR4}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03/2017">03/2017</option>
                 <option value="01/2025" id="selected-cc-exp">01/2025</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON1}-{YEAR2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="3-17">3-17</option>
                 <option value="1-25" id="selected-cc-exp">1-25</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON1}-{YEAR4}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="3-2017">3-2017</option>
                 <option value="1-2025" id="selected-cc-exp">1-2025</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON2}-{YEAR2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03-17">03-17</option>
                 <option value="01-25" id="selected-cc-exp">01-25</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON2}-{YEAR4}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03-2017">03-2017</option>
                 <option value="01-2025" id="selected-cc-exp">01-2025</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {YEAR2}-{MON2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="17-03">17-03</option>
                 <option value="25-01" id="selected-cc-exp">25-01</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {YEAR4}-{MON2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="2017-03">2017-03</option>
                 <option value="2025-01" id="selected-cc-exp">2025-01</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {YEAR4}/{MON2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="2017/3">2017/3</option>
                 <option value="2025/1" id="selected-cc-exp">2025/1</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {MON2}{YEAR2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="0317">0317</option>
                 <option value="0125" id="selected-cc-exp">0125</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Compound cc-exp: {YEAR2}{MON2}",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="1703">1703</option>
                 <option value="2501" id="selected-cc-exp">2501</option>
               </select></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [DEFAULT_CREDITCARD_RECORD],
    expectedOptionElements: [{"cc-exp": "selected-cc-exp"}],
  },
  {
    description: "Fill a cc-exp without cc-exp-month value in the profile",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03/17">03/17</option>
                 <option value="01/25">01/25</option>
               </select></form>`,
    profileData: [Object.assign({}, {
      "guid": "123",
      "cc-exp-year": 2025,
    })],
    expectedResult: [{
      "guid": "123",
      "cc-exp-year": 2025,
    }],
    expectedOptionElements: [],
  },
  {
    description: "Fill a cc-exp without cc-exp-year value in the profile",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp">
                 <option value="03/17">03/17</option>
                 <option value="01/25">01/25</option>
               </select></form>`,
    profileData: [Object.assign({}, {
      "guid": "123",
      "cc-exp-month": 1,
    })],
    expectedResult: [{
      "guid": "123",
      "cc-exp-month": 1,
    }],
    expectedOptionElements: [],
  },
  {
    description: "Fill a cc-exp* without cc-exp-month value in the profile",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp-month">
                 <option value="03">03</option>
                 <option value="01">01</option>
               </select>
               <select autocomplete="cc-exp-year">
                 <option value="17">2017</option>
                 <option value="25">2025</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, {
      "guid": "123",
      "cc-exp-year": 2025,
    })],
    expectedResult: [{
      "guid": "123",
      "cc-exp-year": 2025,
    }],
    expectedOptionElements: [],
  },
  {
    description: "Fill a cc-exp* without cc-exp-year value in the profile",
    document: `<form>
               <input autocomplete="cc-number">
               <select autocomplete="cc-exp-month">
                 <option value="03">03</option>
                 <option value="01">01</option>
               </select>
               <select autocomplete="cc-exp-year">
                 <option value="17">2017</option>
                 <option value="25">2025</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, {
      "guid": "123",
      "cc-exp-month": 1,
    })],
    expectedResult: [{
      "guid": "123",
      "cc-exp-month": 1,
    }],
    expectedOptionElements: [],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm/yy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm/yy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "01/25",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm / yy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm / yy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "01/25",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM / YY].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="MM / YY" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "01/25",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm / yyyy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm / yyyy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "01/2025",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm - yyyy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm - yyyy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "01-2025",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [yyyy-mm].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="yyyy-mm" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "2025-01",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [yyy-mm].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="yyy-mm" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
      "cc-exp": "025-01",
    })],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mmm yyyy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mmm yyyy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm foo yyyy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm foo yyyy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm - - yyyy].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm - - yyyy" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
  },
];

for (let testcase of TESTCASES) {
  add_task(async function() {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                              testcase.document);
    let form = doc.querySelector("form");
    let formLike = FormLikeFactory.createFromForm(form);
    let handler = new FormAutofillHandler(formLike);

    handler.collectFormFields();
    handler.focusedInput = form.elements[0];
    let adaptedRecords = handler.activeSection.getAdaptedProfiles(testcase.profileData);
    Assert.deepEqual(adaptedRecords, testcase.expectedResult);

    if (testcase.expectedOptionElements) {
      testcase.expectedOptionElements.forEach((expectedOptionElement, i) => {
        for (let field in expectedOptionElement) {
          let select = form.querySelector(`[autocomplete=${field}]`);
          let expectedOption = doc.getElementById(expectedOptionElement[field]);
          Assert.notEqual(expectedOption, null);

          let value = testcase.profileData[i][field];
          let cache = handler.activeSection._cacheValue.matchingSelectOption.get(select);
          let targetOption = cache[value] && cache[value].get();
          Assert.notEqual(targetOption, null);

          Assert.equal(targetOption, expectedOption);
        }
      });
    }
  });
}
