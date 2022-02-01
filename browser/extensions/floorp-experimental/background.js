(
    async() => {
    var floorpUA = await browser.aboutConfigPrefs.getPref("general.useragent.override")
    console.log("general.useragent.override: " + floorpUA)
})();

async function getFloorpUserAgent() {
    return await browser.aboutConfigPrefs.getPref("general.useragent.override");
}

async function SetExperimentalFloorpUserAgent(UserAgent) {
    return await browser.aboutConfigPrefs.setCharPref("general.useragent.override", UserAgent);
}

async function SetFloorpExperimentalUserAgent() {
     browser.aboutConfigPrefs.setCharPref("general.useragent.override", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:96.0) Gecko/20100101 Firefox/96.0 Floorp/8.3.6");
}

async function setDefaultUserAgent() {
    browser.aboutConfigPrefs.setCharPref("general.useragent.override", "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:96.0) Gecko/20100101 Firefox/96.0");
}

document.getElementById("UASelector").addEventListener("change", function (e) {
    console.log(e.currentTarget.value)
    var selected = e.currentTarget.value;
    if (selected == "True") {
        setDefaultUserAgent();
        console.log("general.useragent.override: " + floorpUA);
    } 
    else if (selected == "False") {
        SetFloorpExperimentalUserAgent();
        console.log("general.useragent.override: " + floorpUA);
    }
 })


window.addEventListener("load",function(){
    (async () => {
        var pref = await getFloorpUserAgent();
        if (pref == "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:96.0) Gecko/20100101 Firefox/96.0") {
            document.getElementById("UASelector").value = "True";
        } else if (pref == "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:96.0) Gecko/20100101 Firefox/96.0 Floorp/8.3.6") {
            document.getElementById("UASelector").value = "False";
        }
        var floorpUA = await browser.aboutConfigPrefs.getPref("general.useragent.override")
        console.log("general.useragent.override: " + floorpUA)
    })();
})

//ローカライズ

(
	function(){
    var menu_elem_content_1 = document.querySelector("#exper")
    var menu_elem_content_2 = document.querySelector("#header-main")
    var menu_elem_content_3 = document.querySelector("#info")
    var menu_elem_content_4 = document.querySelector("#False")
    var menu_elem_content_5 = document.querySelector("#True")
    var menu_elem_content_6 = document.querySelector("#Feedback")

    menu_elem_content_1.innerText = browser.i18n.getMessage("exper")
    menu_elem_content_2.innerText = browser.i18n.getMessage("header-main")
    menu_elem_content_3.innerText = browser.i18n.getMessage("info")
    menu_elem_content_4.innerText = browser.i18n.getMessage("True")
    menu_elem_content_5.innerText = browser.i18n.getMessage("False") 
    menu_elem_content_6.innerText = browser.i18n.getMessage("Feedback") 
    
}())