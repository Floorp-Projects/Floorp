const window_ = window.wrappedJSObject.window;
const browser_ = window.wrappedJSObject.browser;

if (browser_.runtime.id == "{506e023c-7f2b-40a3-8066-bc5deb40aebe}") {
  let gesturefyController = {
    "commandsId": ["open-bookmarks-sidebar","open-history-sidebar","open-synctabs-sidebar","open-extension-sidebar","close-sidebar","open-browser-manager-sidebar","close-browser-manager-sidebar","toggle-browser-manager-sidebar","show-statusbar","hide-statusbar","toggle-statusbar"],
    "commandSettings":{"open-extension-sidebar":{"UI":function(){
      let extensionList = ""
      let valueJSON = JSON.parse(document.querySelector("#gesturePopupCommandSelect").getAttribute("value") ?? `{"settings":{"extensionId":""}}`)
      let isInSidebar = false
      let selectedExtension = ""
      if(valueJSON.settings != undefined &&  valueJSON.settings.extensionId == "floorp-actions@floorp.ablaze.one"){
        if(JSON.parse(valueJSON.settings.message).action != "open-extension-sidebar"){
          isInSidebar = true
        }else{
          selectedExtension = JSON.parse(valueJSON.settings.message).options.extensionId
        }
      }else{
        isInSidebar = true
      }
      for(let elem of gesturefyController.extensionsInSidebar){
        if(selectedExtension == elem.id){
          isInSidebar = true
          extensionList += `<option value="${elem.id}" selected>${elem.title}</option>`
        } else{
          extensionList += `<option value="${elem.id}">${elem.title}</option>`
        }
      }
        return `
        <div class="select-wrapper">
      <select class="select-field" id="open-extension-sidebar-extension" required>
        <option value="" hidden>${gesturefyController.l10n[isInSidebar ? `gf-floorp-open-extension-sidebar-settings-list-default` : `gf-floorp-open-extension-sidebar-settings-list-unknwon`]}</option>
        ${extensionList}
      </select>
      </div>
        `
      },
      "submit":function(){
        gesturefyController.closeCommandPanel("open-extension-sidebar",{"extensionId":gesturefyController.commandList.querySelector("#open-extension-sidebar-extension").value})
      }
    }},
    "makeSettingPage":function(id){
      const templateFragment = gesturefyController.settingHeader()
      const settingsHeading = templateFragment.getElementById("settingsHeading");
      settingsHeading.title = settingsHeading.textContent = gesturefyController.l10n["gf-floorp-open-extension-sidebar-name"]

      const settingsContent = templateFragment.getElementById("settingsScrollContainer");
      let settingContentPage = document.createRange().createContextualFragment(`
      <form id="settingsForm">
      <div class="cb-setting">
        <span class="cb-setting-name">${gesturefyController.l10n[`gf-floorp-${id}-settings-addons-id`]}</span>
        <p class="cb-setting-description">
          <span>${gesturefyController.l10n[`gf-floorp-${id}-settings-addons-id-description`]}</span>
        </p>
        ${gesturefyController.commandSettings[id]?.UI()}
      </div>
      <button id="settingsSaveButton" type="submit">${gesturefyController.i18n["buttonSave"]}</button>
      <form>
    `)
    settingContentPage.getElementById("settingsForm").onsubmit = gesturefyController.commandSettings[id].submit
      settingsContent.appendChild(settingContentPage)
      return templateFragment.firstElementChild
    },
    "settingsL10n":["gf-floorp-open-extension-sidebar-settings-addons-id","gf-floorp-open-extension-sidebar-settings-addons-id-description","gf-floorp-open-extension-sidebar-settings-list-default","gf-floorp-open-extension-sidebar-settings-list-unknwon"],
    "l10nArg": { "file": ["browser/floorp.ftl"], text: [] },
    "gesturefyI18n":["buttonSave"],
    "i18n":{},
    "setting": null,
    "l10n": null,
    "gestureListElement": null,
    "commandMjs": null,
    "thereIsNotOverlay":true,
    "thereIsSetting":false,
    "extensionsInSidebar":null,
    "slideBack":function(){
            const commandBar = gesturefyController.commandList.getElementById("commandBar");
        const commandsMain = gesturefyController.commandList.getElementById("commandsMain");
        document.querySelector("#gesturePopupCommandSelect")._scrollPosition = commandsMain.scrollTop;

        const commandBarWrapper = gesturefyController.commandList.getElementById("commandBarWrapper");
        const currentPanel = gesturefyController.commandList.querySelector("#settingsPanel");

        const newPanel = gesturefyController.commandList.querySelector("#commandsPanel");
        
        currentPanel.classList.add("cb-init-slide");
        newPanel.style.display = ""
        newPanel.classList.add("cb-init-slide", "cb-slide-left");
    
        currentPanel.addEventListener("transitionend", function removePreviousPanel(event) {
          // prevent event bubbeling
          if (event.currentTarget === event.target) {
            currentPanel.remove()
            currentPanel.classList.remove("cb-init-slide", "cb-slide-right");
            currentPanel.removeEventListener("transitionend", removePreviousPanel);
          }
        });
        newPanel.addEventListener("transitionend", function finishNewPanel(event) {
          // prevent event bubbeling
          if (event.currentTarget === event.target) {
            newPanel.classList.remove("cb-init-slide");
            newPanel.removeEventListener("transitionend", finishNewPanel);
          }
        });
        // trigger reflow
        commandBarWrapper.offsetHeight;
        currentPanel.classList.add("cb-slide-right");
        newPanel.classList.remove("cb-slide-left");
    },
    "settingHeader":function(){
      const templateFragment = document.createRange().createContextualFragment(`
      <div id="settingsPanel" class="cb-panel">
        <div class="cb-head">
          <button id="settingsBackButton" type="button"></button>
          <div id="settingsHeading" class="cb-heading"></div>
        </div>
        <div id="settingsMain" class="cb-main">
          <div id="settingsScrollContainer" class="cb-scroll-container"></div>
        </div>
      </div>
    `);

    // register event handlers
    const settingsBackButton = templateFragment.getElementById("settingsBackButton");
          settingsBackButton.onclick = gesturefyController.slideBack

    return templateFragment
    },
    createCommandListItem: function (id) {
      let returnItem = document.createElement("li")
      returnItem.id = "gf-floorp-" + id
      returnItem.classList.add("cb-command-item")
      returnItem.onclick = this.clickCommandList
      returnItem.onmouseleave = this.commandMouseLeave
      returnItem.onmouseenter = this.commandMouseEnter

      let nameElemBase = document.createElement("div")
      nameElemBase.classList.add("cb-command-container")

      let nameElem = document.createElement("span")
      nameElem.textContent = this.l10n["gf-floorp-" + id + "-name"]
      nameElem.classList.add("cb-command-name")
      nameElemBase.appendChild(nameElem)

      if(id in this.commandSettings){
        let settingElem = document.createElement("span")
        settingElem.classList.add("cb-command-settings-icon")
        nameElemBase.appendChild(settingElem)
      }

      returnItem.appendChild(nameElemBase)

      let descriptionElemBase = document.createElement("div")
      descriptionElemBase.classList.add("cb-command-info")

      let descriptionElem = document.createElement("span")
      descriptionElem.textContent = this.l10n["gf-floorp-" + id + "-description"]
      descriptionElem.classList.add("cb-command-description")
      descriptionElemBase.appendChild(descriptionElem)
      returnItem.appendChild(descriptionElemBase)

      return returnItem
    },
    popupCommandSet:function( id,setting){
      document.querySelector("#gesturePopupCommandSelect").setAttribute("value", `{"name":"SendMessageToOtherAddon","settings":{"extensionId":"floorp-actions@floorp.ablaze.one","message":"{\\"action\\": \\"${id}\\"${setting != undefined ? `,\\"options\\":{${JSON.stringify(JSON.stringify(setting)).slice( 2 ).slice(0, -2 )}}` : ""}}"}}`)
      gesturefyController.commandList.children[1].textContent = gesturefyController.l10n["gf-floorp-" + id + "-name"]
      gesturefyController.commandList.children[1].setAttribute("title",gesturefyController.l10n["gf-floorp-" + id + "-name"])
      if(!(id in this.commandSettings)) gesturefyController.commandList.children[2].classList.remove("has-settings")
      document.querySelector("#gesturePopupLabelInput").setAttribute("placeholder",gesturefyController.l10n["gf-floorp-" + id + "-name"])
      if(document.querySelector("#gesturePopupLabelInput").getAttribute("FloorpGFBeforeText") == document.querySelector("#gesturePopupLabelInput").value || document.querySelector("#gesturePopupLabelInput").value == ""){
        document.querySelector("#gesturePopupLabelInput").value = gesturefyController.l10n["gf-floorp-" + id + "-name"]
        document.querySelector("#gesturePopupLabelInput").style.color = "gray"
      }
      
      document.querySelector("#gesturePopupLabelInput").setAttribute("FloorpGFBeforeText",gesturefyController.l10n["gf-floorp-" + id + "-name"])
      
    },
    observerFunction: function () {
      let valueJSON = JSON.parse(document.querySelector("#gesturePopupCommandSelect").getAttribute("value") ?? `{"settings":{"extensionId":""}}`)
      if(document.querySelector("#gesturePopup").getAttribute("open") === "" && valueJSON.settings != undefined &&  valueJSON.settings.extensionId == "floorp-actions@floorp.ablaze.one"){
        if(JSON.parse(valueJSON.settings.message).action == "open-tree-style-tab"){
          document.querySelector("#gesturePopupCommandSelect").setAttribute("value", `{"name":"SendMessageToOtherAddon","settings":{"extensionId":"floorp-actions@floorp.ablaze.one","message":"{\\"action\\": \\"open-extension-sidebar\\",\\"options\\":{\\"extensionId\\":\\"treestyletab@piro.sakura.ne.jp\\"}}"}}`)
          valueJSON = JSON.parse(document.querySelector("#gesturePopupCommandSelect").getAttribute("value"))
          if(document.querySelector("#gesturePopupLabelInput").value.startsWith("[Floorp]")) document.querySelector("#gesturePopupLabelInput").value = gesturefyController.l10n["gf-floorp-open-extension-sidebar-name"]
        }
        this.popupCommandSet(JSON.parse(valueJSON.settings.message).action,JSON.parse(valueJSON.settings.message).options)
        if(this.l10n["gf-floorp-" + JSON.parse(valueJSON.settings.message).action + "-name"] == document.querySelector("#gesturePopupLabelInput").value){
          document.querySelector("#gesturePopupLabelInput").style.color = "gray"
        }
      }
    },
    showSettingPage:function(id){
      const commandsMain = gesturefyController.commandList.getElementById("commandsMain");
      document.querySelector("#gesturePopupCommandSelect")._scrollPosition = commandsMain.scrollTop;

      const commandBarWrapper = gesturefyController.commandList.getElementById("commandBarWrapper");
      const currentPanel = gesturefyController.commandList.querySelector("#commandsPanel");
      const newPanel = gesturefyController.makeSettingPage(id)
      
      console.log(currentPanel)
      currentPanel.classList.add("cb-init-slide");
      newPanel.classList.add("cb-init-slide", "cb-slide-right");
  
      currentPanel.addEventListener("transitionend", function removePreviousPanel(event) {
        // prevent event bubbeling
        if (event.currentTarget === event.target) {
          currentPanel.style.display = "none"
          currentPanel.classList.remove("cb-init-slide", "cb-slide-left");
          currentPanel.removeEventListener("transitionend", removePreviousPanel);
        }
      });
      newPanel.addEventListener("transitionend", function finishNewPanel(event) {
        // prevent event bubbeling
        if (event.currentTarget === event.target) {
          newPanel.classList.remove("cb-init-slide");
          newPanel.removeEventListener("transitionend", finishNewPanel);
        }
      });
  
      commandBarWrapper.appendChild(newPanel);
      // trigger reflow
      commandBarWrapper.offsetHeight;
  
      currentPanel.classList.add("cb-slide-left");
      newPanel.classList.remove("cb-slide-right");
    },
    clickCommandList:function(event){
      const commandBar = gesturefyController.commandList.getElementById("commandBar");
      if(event.currentTarget.id.replace("gf-floorp-","") in gesturefyController.commandSettings ){
        gesturefyController.showSettingPage(event.currentTarget.id.replace("gf-floorp-",""))
      }else{
        gesturefyController.closeCommandPanel(event.currentTarget.id.replace("gf-floorp-",""))
      }
    },
    closeCommandPanel: function (commandID,settings) {
      const commandBar = gesturefyController.commandList.getElementById("commandBar");
        const overlay = gesturefyController.commandList.getElementById("overlay");
        
        overlay.addEventListener("transitionend", function removeOverlay(event) {
          // prevent the event from firing for child transitions
          if (event.currentTarget === event.target) {
            overlay.removeEventListener("transitionend", removeOverlay);
            overlay.remove();
            gesturefyController.popupCommandSet(commandID,settings)
          }
        });
        commandBar.addEventListener("transitionend", function removeCommandBar(event) {
          // prevent the event from firing for child transitions
          if (event.currentTarget === event.target) {
            commandBar.removeEventListener("transitionend", removeCommandBar);
            commandBar.remove();
          }
        });
        overlay.classList.replace("o-show", "o-hide");
        commandBar.classList.replace("cb-show", "cb-hide");
        document.querySelector("#gesturePopupCommandSelect")._selectedCommand = null;
        document.querySelector("#gesturePopupCommandSelect")._scrollPosition = 0;
      
      

    },
    commandMouseLeave: function (event) {
      const commandItemInfo = event.currentTarget.querySelector(".cb-command-info");
      if (commandItemInfo.style.getPropertyValue("height")) {
        commandItemInfo.style.removeProperty("height");
      }
    },
    commandMouseEnter: function (event) {
      const commandItem = event.currentTarget;
      setTimeout(() => {
        if (commandItem.matches(".cb-command-item:hover")) {
          const commandItemInfo = commandItem.querySelector(".cb-command-info");
          if (!commandItemInfo.style.getPropertyValue("height")) {
            commandItemInfo.style.setProperty("height", commandItemInfo.scrollHeight + "px");
          }
        }
      }, 500);
    },
    commandNameFocus:function(){
      if(document.querySelector("#gesturePopupLabelInput").getAttribute("FloorpGFBeforeText") == document.querySelector("#gesturePopupLabelInput").value){
        document.querySelector("#gesturePopupLabelInput").value = ""
        document.querySelector("#gesturePopupLabelInput").style.color = ""
      }
      
    },
    commandNameBlur:function(){
      if(document.querySelector("#gesturePopupLabelInput").getAttribute("FloorpGFBeforeText") == document.querySelector("#gesturePopupLabelInput").value || document.querySelector("#gesturePopupLabelInput").value == ""){
        document.querySelector("#gesturePopupLabelInput").value = document.querySelector("#gesturePopupLabelInput").getAttribute("FloorpGFBeforeText")
        document.querySelector("#gesturePopupLabelInput").style.color = "gray"
      }
      
    },
    setObserver: function () {
      if (document.querySelector("#gesturePopup") == null) {
        window.setTimeout(this.setObserver, 1000)
      }
      else {
        this.gestureListElement = document.querySelector("#gesturePopup")
        this.observer.observe(this.gestureListElement, { attributes: true })
        this.observerFunction()
        document.querySelector("#gesturePopupLabelInput").addEventListener('focus', this.commandNameFocus);
        document.querySelector("#gesturePopupLabelInput").addEventListener('blur', this.commandNameBlur);
      }
    },
    setCmdListObserver: function () {
      if (document.querySelector("#gesturePopupCommandSelect").shadowRoot == null) {
        window.setTimeout(this.setCmdListObserver, 1000)
      }
      else {
        this.commandList = document.querySelector("#gesturePopupCommandSelect").shadowRoot
        this.cmdListObserver.observe(this.commandList, { childList: true, subtree: true })
      }
    },
    observerCommandSelectFunction: function () {
      let valueJSON = JSON.parse(document.querySelector("#gesturePopupCommandSelect").getAttribute("value") ?? `{"settings":{"extensionId":""}}`)
      if (this.commandList.querySelector(`[data-command="DuplicateTab"]`) != null && this.commandList.querySelector("#FloorpCommands") == null) {
        let baseElem = this.commandList.querySelector(`#commandsScrollContainer`)

        let rootUl = document.createElement("ul")
        rootUl.id = "FloorpCommands"
        rootUl.classList.add("cb-command-group")
        for (let elem of this.commandsId) {
          rootUl.appendChild(this.createCommandListItem(elem))
        }
        baseElem.appendChild(rootUl)
        if(this.thereIsSetting){
          this.thereIsSetting = false
          this.showSettingPage(JSON.parse(valueJSON.settings.message).action)
        }
      }
      if(valueJSON.settings != undefined &&  valueJSON.settings.extensionId == "floorp-actions@floorp.ablaze.one"){
        if(this.commandList.querySelector("#settingsBackButton") != null && this.thereIsNotOverlay){
          this.commandList.querySelector("#settingsBackButton").click()
          if(JSON.parse(valueJSON.settings.message).action in gesturefyController.commandSettings ) this.thereIsSetting = true
        }else if(this.commandList.querySelector(`[name="extensionId"]`) && this.commandList.querySelector(`[name="extensionId"]`).value == "floorp-actions@floorp.ablaze.one"){
          this.commandList.querySelector(`[name="extensionId"]`).value = ""
          this.commandList.querySelector(`[name="message"]`).value = ""
        }else if(this.commandList.querySelector(`.cb-active[data-command="SendMessageToOtherAddon"]`)){
          this.commandList.querySelector(`.cb-active[data-command="SendMessageToOtherAddon"]`).classList.remove("cb-active")
          this.commandList.querySelector(`#gf-floorp-${JSON.parse(valueJSON.settings.message).action}`).classList.add("cb-active")
        }
      }else{
        if(document.querySelector("#gesturePopupLabelInput").getAttribute("FloorpGFBeforeText") == document.querySelector("#gesturePopupLabelInput").value){
          document.querySelector("#gesturePopupLabelInput").value = ""
          document.querySelector("#gesturePopupLabelInput").style.color = "gray"
        }else{
          document.querySelector("#gesturePopupLabelInput").style.color = ""
        }
        
        document.querySelector("#gesturePopupLabelInput").removeAttribute("FloorpGFBeforeText")
      }
      this.thereIsNotOverlay = (this.commandList.querySelector("#overlay") == null)
      

    }
  };
  gesturefyController.observer = new MutationObserver(gesturefyController.observerFunction.bind(gesturefyController));
  gesturefyController.cmdListObserver = new MutationObserver(gesturefyController.observerCommandSelectFunction.bind(gesturefyController));

  (async () => {
    //gesturefyController.commandMjs = (await import(location.origin + "/core/models/command.mjs")).default
    //gesturefyController.setting = await browser_.storage.local.get()
    for(const i of gesturefyController.commandsId){
      gesturefyController.l10nArg.text.push("gf-floorp-" + i + "-name")
      gesturefyController.l10nArg.text.push("gf-floorp-" + i + "-description")
    }
    for(const i of gesturefyController.settingsL10n){
      gesturefyController.l10nArg.text.push(i)
      console.log(i)
    }

    let l10nArr = await browser.runtime.sendMessage({data:gesturefyController.l10nArg,type:"l10n"})
    let l10nTexts = {}
    for (let i = 0; i < l10nArr.length; i++) {
      l10nTexts[gesturefyController.l10nArg.text[i]] = l10nArr[i]
    }

    for(const i of gesturefyController.gesturefyI18n){
      gesturefyController.i18n[i] = browser_.i18n.getMessage(i)
    }

    gesturefyController.extensionsInSidebar = await browser.runtime.sendMessage({data:"",type:"extensionsInSidebar"})

    gesturefyController.l10n = l10nTexts
    gesturefyController.setObserver()
    gesturefyController.setCmdListObserver()
  })()
}
