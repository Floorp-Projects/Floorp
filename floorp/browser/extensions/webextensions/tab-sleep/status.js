function getStatusData() {
    function handleResponse(message) {
        if (message && message["response"] === "status-data") {
            let status = message["data"]["status"];
            let keys = Object.keys(status);
            for (let key of keys) {
                let elem = document.getElementById(key);
                if (elem) {
                    elem.querySelector(".value").innerText = status[key];
                }
            }
        }
    }
    
    function handleError(error) {
        console.error(error);
    }

    var sending = browser.runtime.sendMessage({
        request: "status-data"
    });
    sending.then(handleResponse, handleError);
}

function getTabsLastActivityInfoData() {
    function handleResponse(message) {
        if (message && message["response"] === "tabs-last-activity-data") {
            let tabsActivityInfoBox = document.querySelector(".tabsLastActivityInfo-box");
            let template = document.querySelector(".tabLastActivityInfo-box.box-template");
            let tabsLastActivity = message["data"]["tabsLastActivity"];
            tabsActivityInfoBox.innerHTML = "";
            for (let tabLastActivity of tabsLastActivity) {
                let title = tabLastActivity["title"];
                let lastActivity = tabLastActivity["lastActivity"];
                let elem = template.cloneNode(true);
                elem.classList.remove("box-template");
                elem.querySelector(".title").innerText = title;
                elem.querySelector(".value").innerText = lastActivity;
                tabsActivityInfoBox.appendChild(elem);
            }
        }
    }
    
    function handleError(error) {
        console.error(error);
    }

    var sending = browser.runtime.sendMessage({
        request: "tabs-last-activity-data"
    });
    sending.then(handleResponse, handleError);
}

document.addEventListener("DOMContentLoaded", function() {
    getStatusData();
    getTabsLastActivityInfoData();
}, { once: true });

setInterval(function() {
    getStatusData();
}, 1000);

setInterval(function() {
    getTabsLastActivityInfoData();
}, 1000);