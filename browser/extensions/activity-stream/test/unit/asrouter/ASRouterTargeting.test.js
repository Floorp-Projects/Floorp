import {ASRouterTargeting} from "lib/ASRouterTargeting.jsm";
const ONE_DAY = 24 * 60 * 60 * 1000;

// Note that tests for the ASRouterTargeting environment can be found in
// test/functional/mochitest/browser_asrouter_targeting.js

describe("ASRouterTargeting#isBelowFrequencyCap", () => {
  describe("lifetime frequency caps", () => {
    it("should return true if .frequency is not defined on the message", () => {
      const message = {id: "msg1"};
      const impressions = [0, 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isTrue(result);
    });
    it("should return true if there are no impressions", () => {
      const message = {id: "msg1", frequency: {lifetime: 10, custom: [{period: ONE_DAY, cap: 2}]}};
      const impressions = [];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isTrue(result);
    });
    it("should return true if the # of impressions is less than .frequency.lifetime", () => {
      const message = {id: "msg1", frequency: {lifetime: 3}};
      const impressions = [0, 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isTrue(result);
    });
    it("should return false if the # of impressions is equal to .frequency.lifetime", () => {
      const message = {id: "msg1", frequency: {lifetime: 2}};
      const impressions = [0, 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isFalse(result);
    });
    it("should return false if the # of impressions is greater than .frequency.lifetime", () => {
      const message = {id: "msg1", frequency: {lifetime: 2}};
      const impressions = [0, 1, 2];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isFalse(result);
    });
  });
  describe("custom frequency caps", () => {
    let sandbox;
    let clock;
    beforeEach(() => {
      sandbox = sinon.sandbox.create();
      clock = sandbox.useFakeTimers();
    });
    afterEach(() => {
      sandbox.restore();
    });
    it("should return true if impressions in the time period < the cap and total impressions < the lifetime cap", () => {
      clock.tick(ONE_DAY + 10);
      const message = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}], lifetime: 3}};
      const impressions = [0, ONE_DAY + 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isTrue(result);
    });
    it("should return false if impressions in the time period > the cap and total impressions < the lifetime cap", () => {
      clock.tick(200);
      const message = {id: "msg1", frequency: {custom: [{period: 100, cap: 2}], lifetime: 3}};
      const impressions = [0, 160, 161];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isFalse(result);
    });
    it("should return false if impressions in one of the time periods > the cap and total impressions < the lifetime cap", () => {
      clock.tick(ONE_DAY + 200);
      const messageTrue = {id: "msg2", frequency: {custom: [{period: 100, cap: 2}]}};
      const messageFalse = {id: "msg1", frequency: {custom: [{period: 100, cap: 2}, {period: ONE_DAY, cap: 3}]}};
      const impressions = [0, ONE_DAY + 160, ONE_DAY - 100, ONE_DAY - 200];
      assert.isTrue(ASRouterTargeting.isBelowFrequencyCap(messageTrue, impressions));
      assert.isFalse(ASRouterTargeting.isBelowFrequencyCap(messageFalse, impressions));
    });
    it("should return false if impressions in the time period < the cap and total impressions > the lifetime cap", () => {
      clock.tick(ONE_DAY + 10);
      const message = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}], lifetime: 3}};
      const impressions = [0, 1, 2, 3, ONE_DAY + 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isFalse(result);
    });
    it("should return true if daily impressions < the daily cap and there is no lifetime cap", () => {
      clock.tick(ONE_DAY + 10);
      const message = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}]}};
      const impressions = [0, 1, 2, 3, ONE_DAY + 1];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isTrue(result);
    });
    it("should return false if daily impressions > the daily cap and there is no lifetime cap", () => {
      clock.tick(ONE_DAY + 10);
      const message = {id: "msg1", frequency: {custom: [{period: ONE_DAY, cap: 2}]}};
      const impressions = [0, 1, 2, 3, ONE_DAY + 1, ONE_DAY + 2, ONE_DAY + 3];
      const result = ASRouterTargeting.isBelowFrequencyCap(message, impressions);
      assert.isFalse(result);
    });
    it("should allow the 'daily' alias for period", () => {
      clock.tick(ONE_DAY + 10);
      const message = {id: "msg1", frequency: {custom: [{period: "daily", cap: 2}]}};
      assert.isFalse(ASRouterTargeting.isBelowFrequencyCap(message, [0, 1, 2, 3, ONE_DAY + 1, ONE_DAY + 2, ONE_DAY + 3]));
      assert.isTrue(ASRouterTargeting.isBelowFrequencyCap(message, [0, 1, 2, 3, ONE_DAY + 1]));
    });
  });
});
