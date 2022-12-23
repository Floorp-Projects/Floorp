function getData() {
    function handleResponse(message) {
        if (message["response"] === "status-data") {
            let status = message["data"]["status"];
            let keys = Object.keys(status);
            for (let key of keys) {
                document.getElementById(key).querySelector(".value").innerText = status[key];
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

document.addEventListener("DOMContentLoaded", function() {
    getData();
}, { once: true });

setInterval(function() {
    getData();
}, 10000)