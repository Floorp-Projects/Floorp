const window_ = window.wrappedJSObject.window;
const browser_ = window.wrappedJSObject.browser;

if (browser_.runtime.id == "{506e023c-7f2b-40a3-8066-bc5deb40aebe}") {
  let gesturefyController = {
    "commandsId": ["open-tree-style-tab","open-bookmarks-sidebar","open-history-sidebar","open-synctabs-sidebar","close-sidebar","open-browser-manager-sidebar","close-browser-manager-sidebar","toggle-browser-manager-sidebar","show-statusbar","hide-statusbar","toggle-statusbar"],
    "commandObject": {
      "open-tree-style-tab": `{"name":"SendMessageToOtherAddon","settings":{"extensionId":"floorp-actions@floorp.ablaze.one","message":"{\\"action\\": \\"open-tree-style-tab\\"}"}}`
    },
    "l10nArg": { "file": ["browser/floorp.ftl"], text: [] },
    "setting": null,
    "l10n": null,
    "gestureListElement": null,
    "commandMjs": null,
    "thereIsNotOverlay":true,
    createCommandListItem: function (id) {
      let returnItem = document.createElement("li")
      returnItem.id = "gf-floorp-" + id
      returnItem.classList.add("cb-command-item")
      returnItem.onclick = this.closeCommandPanel
      returnItem.onmouseleave = this.commandMouseLeave
      returnItem.onmouseenter = this.commandMouseEnter

      let nameElemBase = document.createElement("div")
      nameElemBase.classList.add("cb-command-container")

      let nameElem = document.createElement("span")
      nameElem.textContent = this.l10n["gf-floorp-" + id + "-name"]
      nameElem.classList.add("cb-command-name")
      nameElemBase.appendChild(nameElem)
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
    popupCommandSet:function( id){
      document.querySelector("#gesturePopupCommandSelect").setAttribute("value", `{"name":"SendMessageToOtherAddon","settings":{"extensionId":"floorp-actions@floorp.ablaze.one","message":"{\\"action\\": \\"${id}\\"}"}}`)
      gesturefyController.commandList.children[1].textContent = gesturefyController.l10n["gf-floorp-" + id + "-name"]
      gesturefyController.commandList.children[1].setAttribute("title",gesturefyController.l10n["gf-floorp-" + id + "-name"])
      gesturefyController.commandList.children[2].classList.remove("has-settings")
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
        this.popupCommandSet(JSON.parse(valueJSON.settings.message).action)
        if(this.l10n["gf-floorp-" + JSON.parse(valueJSON.settings.message).action + "-name"] == document.querySelector("#gesturePopupLabelInput").value){
          document.querySelector("#gesturePopupLabelInput").style.color = "gray"
        }
      }
    },
    closeCommandPanel: function (event) {

      const overlay = gesturefyController.commandList.getElementById("overlay");
      const commandBar = gesturefyController.commandList.getElementById("commandBar");

      let commandIdTemp = event.currentTarget.id
      overlay.addEventListener("transitionend", function removeOverlay(event) {
        // prevent the event from firing for child transitions
        if (event.currentTarget === event.target) {
          overlay.removeEventListener("transitionend", removeOverlay);
          overlay.remove();
          gesturefyController.popupCommandSet(commandIdTemp.replace("gf-floorp-",""))
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
      if (this.commandList.querySelector(`[data-command="DuplicateTab"]`) != null && this.commandList.querySelector("#FloorpCommands") == null) {
        let baseElem = this.commandList.querySelector(`#commandsScrollContainer`)

        let rootUl = document.createElement("ul")
        rootUl.id = "FloorpCommands"
        rootUl.classList.add("cb-command-group")
        for (let elem of this.commandsId) {
          rootUl.appendChild(this.createCommandListItem(elem))
        }


        baseElem.appendChild(rootUl)
      }
      let valueJSON = JSON.parse(document.querySelector("#gesturePopupCommandSelect").getAttribute("value") ?? `{"settings":{"extensionId":""}}`)
      if(valueJSON.settings != undefined &&  valueJSON.settings.extensionId == "floorp-actions@floorp.ablaze.one"){
        if(this.commandList.querySelector("#settingsBackButton") != null && this.thereIsNotOverlay){
          this.commandList.querySelector("#settingsBackButton").click()
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

    let l10nArr = await browser.runtime.sendMessage(gesturefyController.l10nArg)
    let l10nTexts = {}
    for (let i = 0; i < l10nArr.length; i++) {
      l10nTexts[gesturefyController.l10nArg.text[i]] = l10nArr[i]
    }

    gesturefyController.l10n = l10nTexts
    gesturefyController.setObserver()
    gesturefyController.setCmdListObserver()
  })()
}