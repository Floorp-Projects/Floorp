"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.crypto", function () {
  "use strict";

  var expect = chai.expect;
  var sandbox;

  beforeEach(function () {
    sandbox = sinon.sandbox.create();});


  afterEach(function () {
    sandbox.restore();
    loop.crypto.setRootObject(window);});


  describe("#isSupported", function () {
    it("should return true by default", function () {
      expect(loop.crypto.isSupported()).eql(true);});


    it("should return false if crypto isn't supported", function () {
      loop.crypto.setRootObject({});

      expect(loop.crypto.isSupported()).eql(false);});});



  describe("#generateKey", function () {
    it("should throw if web crypto is not available", function () {
      loop.crypto.setRootObject({});

      expect(function () {
        loop.crypto.generateKey();}).
      to.Throw(/not supported/);});


    it("should generate a key", function () {
      // The key is a random string, so we can't really test much else.
      return expect(loop.crypto.generateKey()).to.eventually.be.a("string");});});



  describe("#encryptBytes", function () {
    it("should throw if web crypto is not available", function () {
      loop.crypto.setRootObject({});

      expect(function () {
        loop.crypto.encryptBytes();}).
      to.Throw(/not supported/);});


    it("should encrypt an object with a specific key", function () {
      return expect(loop.crypto.encryptBytes("Wt2-bZKeHO2wnaq00ZM6Nw", 
      JSON.stringify({ test: true }))).to.eventually.be.a("string");});});



  describe("#decryptBytes", function () {
    it("should throw if web crypto is not available", function () {
      loop.crypto.setRootObject({});

      expect(function () {
        loop.crypto.decryptBytes();}).
      to.Throw(/not supported/);});


    it("should decypt an object via a specific key", function () {
      var key = "Wt2-bZKeHO2wnaq00ZM6Nw";
      var encryptedContext = "XvN9FDEm/GtE/5Bx5ezpn7JVDeZrtwOJy2CBjTGgJ4L33HhHOqEW+5k=";

      return expect(loop.crypto.decryptBytes(key, encryptedContext)).to.eventually.eql(JSON.stringify({ test: true }));});


    it("should fail if the key didn't work", function () {
      var bad = "Bad-bZKeHO2wnaq00ZM6Nw";
      var encryptedContext = "TGZaAE3mqsBFK0GfheZXXDCaRKXJmIKJ8WzF0KBEl4Aldzf3iYlAsLQdA8XSXXvtJR2UYz+f";

      return expect(loop.crypto.decryptBytes(bad, encryptedContext)).to.be.rejected;});});



  describe("Full cycle", function () {
    it("should be able to encrypt and decypt in a full cycle", function (done) {
      var context = JSON.stringify({ 
        contextObject: true, 
        UTF8String: "对话" });


      return loop.crypto.generateKey().then(function (key) {
        loop.crypto.encryptBytes(key, context).then(function (encryptedContext) {
          loop.crypto.decryptBytes(key, encryptedContext).then(function (decryptedContext) {
            expect(decryptedContext).eql(context);
            done();}).
          catch(function (error) {
            done(error);});}).

        catch(function (error) {
          done(error);});}).

      catch(function (error) {
        done(error);});});});});
