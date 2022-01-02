(() => {
    "use strict";
    const t = {
      animations: !0,
      themeIntegration: !0,
      compactMode: 1,
      compactPins: !0,
      switchLastActiveTab: !0,
      switchByScrolling: 0,
      notifyClosingManyTabs: !0
    };
    new class {
      constructor() {
        const t = ["optionsAppearanceTitle", "optionsAnimations", "optionsCompactMode", "optionsCompactModeStrict", "optionsCompactModeDynamic", "optionsCompactModeOff", "optionsCompactModeExplanation", "optionsCompactPins", "optionsThemeIntegration", "optionsThemeIntegrationExplanation", "optionsBehaviorTitle", "optionsSwitchLastActiveTabExplanation", "optionsSwitchLastActiveTab", "optionsSwitchByScrolling", "optionsSwitchByScrollingWithCtrl", "optionsSwitchByScrollingAlways", "optionsSwitchByScrollingNever", "optionsSwitchByScrollingWithCtrlExplanation", "optionsNotifyClosingManyTabs"],
          e = [];
        requestAnimationFrame((() => {
          for (const o of t) e.push([o, document.getElementById(o)]);
          const o = "optionsNotifyClosingManyTabsExplanation",
            s = document.getElementById(o);
          for (const [t, o] of e) {
            const e = document.createTextNode(browser.i18n.getMessage(t));
            o.appendChild(e)
          }
          s.appendChild(document.createTextNode(browser.i18n.getMessage(o, 5))), document.body.classList.add("loaded"), this.setupState()
        }))
      }
      setupState() {
        browser.storage.sync.get(t).then((t => {
          requestAnimationFrame((() => {
            for (const e of Object.entries(t)) {
              const t = document.getElementById(e[0]);
              "customCSS" === e[0] ? t.value = e[1] : "compactMode" === e[0] || "switchByScrolling" === e[0] ? document.querySelector(`[name="${e[0]}"][value="${parseInt(e[1])}"]`).checked = !0 : (t.checked = e[1], "useCustomCSS" === e[0] && this.updateCustomCSSEnabled(e[1]))
            }
            this.setupListeners()
          }))
        }))
      }
      setupListeners() {
        document.body.addEventListener("change", (t => {
          if ("INPUT" === t.target.tagName)
            if ("radio" === t.target.type) browser.storage.sync.set({
              [t.target.name]: parseInt(t.target.value)
            });
            else if ("checkbox" === t.target.type && (browser.storage.sync.set({
              [t.target.id]: t.target.checked
            }), "useCustomCSS" === t.target.id)) {
            const e = t.target.checked;
            this.updateCustomCSSEnabled(e), e || this.saveCustomCSS()
          }
        })), document.getElementById("optionsSaveCustomCSS").addEventListener("click", (() => this.saveCustomCSS())), document.getElementById("customCSS").addEventListener("keydown", (t => {
          t.ctrlKey && "Enter" === t.key && this.saveCustomCSS()
        }))
      }
      saveCustomCSS() {
        browser.storage.sync.set({
          customCSS: document.getElementById("customCSS").value
        })
      }
      updateCustomCSSEnabled(t) {
        document.getElementById("customCSS").disabled = !t, document.getElementById("optionsSaveCustomCSS").disabled = !t
      }
    }
  })();