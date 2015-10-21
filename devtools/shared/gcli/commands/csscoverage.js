/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci } = require("chrome");

loader.lazyGetter(this, "gDevTools", () => require("resource://devtools/client/framework/gDevTools.jsm").gDevTools);

const domtemplate = require("gcli/util/domtemplate");
const csscoverage = require("devtools/server/actors/csscoverage");
const l10n = csscoverage.l10n;

const { XPCOMUtils } = require("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Chart", "resource://devtools/client/shared/widgets/Chart.jsm");

/**
 * The commands/converters for GCLI
 */
exports.items = [
  {
    name: "csscoverage",
    hidden: true,
    description: l10n.lookup("csscoverageDesc"),
  },
  {
    item: "command",
    runAt: "client",
    name: "csscoverage start",
    hidden: true,
    description: l10n.lookup("csscoverageStartDesc2"),
    params: [
      {
        name: "noreload",
        type: "boolean",
        description: l10n.lookup("csscoverageStartNoReloadDesc"),
        manual: l10n.lookup("csscoverageStartNoReloadManual")
      }
    ],
    exec: function*(args, context) {
      let usage = yield csscoverage.getUsage(context.environment.target);
      if (usage == null) {
        throw new Error(l10n.lookup("csscoverageNoRemoteError"));
      }
      yield usage.start(context.environment.chromeWindow,
                        context.environment.target, args.noreload);
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "csscoverage stop",
    hidden: true,
    description: l10n.lookup("csscoverageStopDesc2"),
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);
      if (usage == null) {
        throw new Error(l10n.lookup("csscoverageNoRemoteError"));
      }
      yield usage.stop();
      yield gDevTools.showToolbox(target, "styleeditor");
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "csscoverage oneshot",
    hidden: true,
    description: l10n.lookup("csscoverageOneShotDesc2"),
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);
      if (usage == null) {
        throw new Error(l10n.lookup("csscoverageNoRemoteError"));
      }
      yield usage.oneshot();
      yield gDevTools.showToolbox(target, "styleeditor");
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "csscoverage toggle",
    hidden: true,
    description: l10n.lookup("csscoverageToggleDesc2"),
    state: {
      isChecked: function(target) {
        return csscoverage.getUsage(target).then(usage => {
          return usage.isRunning();
        });
      },
      onChange: function(target, handler) {
        csscoverage.getUsage(target).then(usage => {
          this.handler = ev => { handler("state-change", ev); };
          usage.on("state-change", this.handler);
        });
      },
      offChange: function(target, handler) {
        csscoverage.getUsage(target).then(usage => {
          usage.off("state-change", this.handler);
          this.handler = undefined;
        });
      },
    },
    exec: function*(args, context) {
      let target = context.environment.target;
      let usage = yield csscoverage.getUsage(target);
      if (usage == null) {
        throw new Error(l10n.lookup("csscoverageNoRemoteError"));
      }

      yield usage.toggle(context.environment.chromeWindow,
                         context.environment.target);
    }
  },
  {
    item: "command",
    runAt: "client",
    name: "csscoverage report",
    hidden: true,
    description: l10n.lookup("csscoverageReportDesc2"),
    exec: function*(args, context) {
      let usage = yield csscoverage.getUsage(context.environment.target);
      if (usage == null) {
        throw new Error(l10n.lookup("csscoverageNoRemoteError"));
      }

      return {
        isTypedData: true,
        type: "csscoveragePageReport",
        data: yield usage.createPageReport()
      };
    }
  },
  {
    item: "converter",
    from: "csscoveragePageReport",
    to: "dom",
    exec: function*(csscoveragePageReport, context) {
      let target = context.environment.target;

      let toolbox = yield gDevTools.showToolbox(target, "styleeditor");
      let panel = toolbox.getCurrentPanel();

      let host = panel._panelDoc.querySelector(".csscoverage-report");
      let templ = panel._panelDoc.querySelector(".csscoverage-template");

      templ = templ.cloneNode(true);
      templ.hidden = false;

      let data = {
        preload: csscoveragePageReport.preload,
        unused: csscoveragePageReport.unused,
        summary: csscoveragePageReport.summary,
        onback: () => {
          // The back button clears and hides .csscoverage-report
          while (host.hasChildNodes()) {
            host.removeChild(host.firstChild);
          }
          host.hidden = true;
        }
      };

      let addOnClick = rule => {
        rule.onclick = () => {
          panel.selectStyleSheet(rule.url, rule.start.line);
        };
      };

      data.preload.forEach(page => {
        page.rules.forEach(addOnClick);
      });
      data.unused.forEach(page => {
        page.rules.forEach(addOnClick);
      });

      let options = { allowEval: true, stack: "styleeditor.xul" };
      domtemplate.template(templ, data, options);

      while (templ.hasChildNodes()) {
        host.appendChild(templ.firstChild);
      }

      // Create a new chart.
      let container = host.querySelector(".csscoverage-report-chart");
      let chart = Chart.PieTable(panel._panelDoc, {
        diameter: 200, // px
        title: "CSS Usage",
        data: [
          { size: data.summary.preload, label: "Used Preload" },
          { size: data.summary.used, label: "Used" },
          { size: data.summary.unused, label: "Unused" }
        ]
      });
      container.appendChild(chart.node);

      host.hidden = false;
    }
  }
];
