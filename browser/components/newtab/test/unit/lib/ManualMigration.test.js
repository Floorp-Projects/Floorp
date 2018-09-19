import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";
import {GlobalOverrider} from "test/unit/utils";
import {ManualMigration} from "lib/ManualMigration.jsm";

const FAKE_MIGRATION_EXPIRED = false;
const FAKE_MIGRATION_LAST_SHOWN_DATE = 999;
const FAKE_MIGRATION_REMAINING_DAYS = 4;

describe("ManualMigration", () => {
  let dispatch;
  let instance;
  let globals;
  let sandbox;

  let migrationWizardStub;
  let fakeProfileAge;
  let prefs;

  beforeEach(() => {
    migrationWizardStub = sinon.stub();
    let fakeMigrationUtils = {
      showMigrationWizard: migrationWizardStub,
      MIGRATION_ENTRYPOINT_NEWTAB: "MIGRATION_ENTRYPOINT_NEWTAB",
    };

    fakeProfileAge = function() {};
    fakeProfileAge.prototype = {
      get created() {
        return new Promise(resolve => {
          resolve(Date.now());
        });
      },
    };

    sandbox = sinon.sandbox.create();
    globals = new GlobalOverrider();
    globals.set("MigrationUtils", fakeMigrationUtils);
    globals.set("ProfileAge", fakeProfileAge);
    sandbox.spy(global.Services.obs, "addObserver");
    sandbox.spy(global.Services.obs, "removeObserver");

    instance = new ManualMigration();
    prefs = {
      "migrationExpired": FAKE_MIGRATION_EXPIRED,
      "migrationLastShownDate": FAKE_MIGRATION_LAST_SHOWN_DATE,
      "migrationRemainingDays": FAKE_MIGRATION_REMAINING_DAYS,
    };
    dispatch = sinon.stub();
    instance.store = {
      dispatch,
      getState: sinon.stub().returns({Prefs: {values: prefs}}),
    };
  });

  afterEach(() => {
    sandbox.restore();
    globals.restore();
  });

  it("should set an event listener for Migration:Ended", () => {
    assert.calledOnce(global.Services.obs.addObserver);
    assert.calledWith(global.Services.obs.addObserver, instance, "Migration:Ended");
  });

  describe("onAction", () => {
    it("should call expireIfNecessary on PREFS_INITIAL_VALUE", () => {
      const action = {
        type: at.PREFS_INITIAL_VALUES,
        data: {migrationExpired: true},
      };

      const expireStub = sinon.stub(instance, "expireIfNecessary");
      instance.onAction(action);

      assert.calledOnce(expireStub);
      assert.calledWithExactly(expireStub, action.data.migrationExpired);
    });
    it("should call launch the migration wizard on MIGRATION_START", () => {
      const action = {
        type: at.MIGRATION_START,
        _target: {browser: {ownerGlobal: "browser.xul"}},
        data: {migrationExpired: false},
      };

      instance.onAction(action);

      assert.calledOnce(migrationWizardStub);
      assert.calledWithExactly(migrationWizardStub, action._target.browser.ownerGlobal, ["MIGRATION_ENTRYPOINT_NEWTAB"]);
    });
    it("should set migrationStatus to true on MIGRATION_CANCEL", () => {
      const action = {type: at.MIGRATION_CANCEL};

      const setStatusStub = sinon.spy(instance, "expireMigration");
      instance.onAction(action);

      assert.calledOnce(setStatusStub);
      assert.calledOnce(dispatch);
      assert.calledWithExactly(dispatch, ac.SetPref("migrationExpired", true));
    });
    it("should set migrationStatus when isMigrationMessageExpired is true", async () => {
      const setStatusStub = sinon.stub(instance, "expireMigration");
      sinon.stub(instance, "isMigrationMessageExpired")
        .returns(Promise.resolve(true));

      await instance.expireIfNecessary(false);

      assert.calledOnce(setStatusStub);
    });
    it("should call isMigrationMessageExpired if migrationExpired is false", () => {
      const action = {
        type: at.PREFS_INITIAL_VALUES,
        data: {migrationExpired: false},
      };

      const stub = sinon.stub(instance, "isMigrationMessageExpired");
      instance.onAction(action);

      assert.calledOnce(stub);
    });
    describe("isMigrationMessageExpired", () => {
      it("should check migrationLastShownDate (case: today)", async () => {
        let today = new Date();
        today = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        prefs.migrationLastShownDate = today;

        const result = await instance.isMigrationMessageExpired();
        assert.equal(result, false);
      });
      it("should return false if lastShownDate is today", async () => {
        let today = new Date();
        today = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        prefs.isMigrationMessageExpired = today;

        const result = await instance.isMigrationMessageExpired();
        assert.equal(result, false);
      });
      it("should check migrationLastShownDate (case: yesterday)", async () => {
        const action = {
          type: at.PREFS_INITIAL_VALUES,
          data: {migrationExpired: false},
        };
        let today = new Date();
        let yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
        prefs.migrationLastShownDate = yesterday.valueOf() / 1000;
        prefs.migrationRemainingDays = 4;

        await instance.onAction(action);

        assert.calledTwice(instance.store.getState);
      });
      it("should update the migration prefs", async () => {
        const action = {
          type: at.PREFS_INITIAL_VALUES,
          data: {migrationExpired: false},
        };
        let today = new Date();
        let yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
        today = new Date(today.getFullYear(), today.getMonth(), today.getDate());
        prefs.migrationLastShownDate = yesterday.valueOf() / 1000;
        prefs.migrationRemainingDays = 4;

        await instance.onAction(action);

        assert.calledTwice(dispatch);
        assert.calledWithExactly(dispatch, ac.SetPref("migrationRemainingDays", 3));
        assert.calledWithExactly(dispatch, ac.SetPref("migrationLastShownDate", today.valueOf() / 1000));
      });
      it("should return true if remainingDays reaches 0", async () => {
        let today = new Date();
        let yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
        prefs.migrationLastShownDate = yesterday.valueOf() / 1000;
        prefs.migrationRemainingDays = 1;

        const result = await instance.isMigrationMessageExpired();
        assert.calledWithExactly(dispatch, ac.SetPref("migrationRemainingDays", 0));
        assert.equal(result, true);
      });
      it("should return false if profile age < 3", async () => {
        let today = new Date();
        let yesterday = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 1);
        prefs.migrationLastShownDate = yesterday.valueOf() / 1000;
        prefs.migrationRemainingDays = 2;

        const result = await instance.isMigrationMessageExpired();
        assert.equal(result, false);
      });
      it("should return true if profile age > 3", async () => {
        let today = new Date();
        let someDaysAgo = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 4);
        fakeProfileAge.prototype = {
          get created() {
            return new Promise(resolve => {
              resolve(someDaysAgo.valueOf());
            });
          },
        };
        prefs.migrationLastShownDate = someDaysAgo.valueOf() / 1000;
        prefs.migrationRemainingDays = 2;

        const result = await instance.isMigrationMessageExpired();
        assert.equal(result, true);
      });
      it("should return early and not check prefs if profile age > 3", async () => {
        let today = new Date();
        let someDaysAgo = new Date(today.getFullYear(), today.getMonth(), today.getDate() - 4);
        fakeProfileAge.prototype = {
          get created() {
            return new Promise(resolve => {
              resolve(someDaysAgo.valueOf());
            });
          },
        };

        const result = await instance.isMigrationMessageExpired();

        assert.equal(instance.store.getState.callCount, 0);
        assert.equal(instance.store.getState.callCount, 0);
        assert.equal(result, true);
      });
    });
  });
  it("should have observe as a proxy for setMigrationStatus", () => {
    const setStatusStub = sinon.stub(instance, "expireMigration");
    instance.observe();

    assert.calledOnce(setStatusStub);
  });
  it("should remove observer at uninit", () => {
    const uninitSpy = sinon.spy(instance, "uninit");
    const action = {type: at.UNINIT};

    instance.onAction(action);

    assert.calledOnce(uninitSpy);
    assert.calledOnce(global.Services.obs.removeObserver);
    assert.calledWith(global.Services.obs.removeObserver, instance, "Migration:Ended");
  });
});
