var message = document.getElementById('message');

function logMessage() {
  console.log('message is: ' + message.value);
}

message.addEventListener('focus', function() {
  message.value = '';
}, false);

var logMessageButton = document.getElementById('log-message');

logMessageButton.addEventListener('click', function() {
  logMessage();
}, false);
