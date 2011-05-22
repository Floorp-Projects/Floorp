from mod_pywebsocket import msgutil

import time
import sys

# see the list of tests in test_websocket.html

def web_socket_do_extra_handshake(request):
  # must set request.ws_protocol to the selected version from ws_requested_protocols
  request.ws_protocol = request.ws_requested_protocols[0]

  if request.ws_protocol == "test 2.1":
    time.sleep(5)
    pass
  elif request.ws_protocol == "test 9":
    time.sleep(5)
    pass
  elif request.ws_protocol == "test 10":
    time.sleep(5)
    pass
  elif request.ws_protocol == "test 19":
    raise ValueError('Aborting (test 19)')
  elif request.ws_protocol == "test 20" or request.ws_protocol == "test 17":
    time.sleep(10)
    pass
  elif request.ws_protocol == "test 22":
    time.sleep(60)
    pass
  else:
    pass

def web_socket_transfer_data(request):
  if request.ws_protocol == "test 2.1" or request.ws_protocol == "test 2.2":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 6":
    resp = "wrong message"
    if msgutil.receive_message(request) == "1":
      resp = "2"
    msgutil.send_message(request, resp.decode('utf-8'))
    resp = "wrong message"
    if msgutil.receive_message(request) == "3":
      resp = "4"
    msgutil.send_message(request, resp.decode('utf-8'))
    resp = "wrong message"
    if msgutil.receive_message(request) == "5":
      resp = "あいうえお"
    msgutil.send_message(request, resp.decode('utf-8'))
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 7":
    try:
      while not request.client_terminated:
        msgutil.receive_message(request)
    except msgutil.ConnectionTerminatedException, e:
      pass
    msgutil.send_message(request, "server data")
    msgutil.send_message(request, "server data")
    msgutil.send_message(request, "server data")
    msgutil.send_message(request, "server data")
    msgutil.send_message(request, "server data")
    time.sleep(30)
    msgutil.close_connection(request, True)
  elif request.ws_protocol == "test 10":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 11":
    resp = "wrong message"
    if msgutil.receive_message(request) == "client data":
      resp = "server data"
    msgutil.send_message(request, resp.decode('utf-8'))
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 12":
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 13":
    # first one binary message containing the byte 0x61 ('a')
    request.connection.write('\xff\x01\x61')
    # after a bad utf8 message
    request.connection.write('\x01\x61\xff')
    msgutil.close_connection(request)
  elif request.ws_protocol == "test 14":
    msgutil.close_connection(request)
    msgutil.send_message(request, "server data")
  elif request.ws_protocol == "test 15":
    msgutil.close_connection(request, True)
    return
  elif request.ws_protocol == "test 17" or request.ws_protocol == "test 21":
    time.sleep(5)
    resp = "wrong message"
    if msgutil.receive_message(request) == "client data":
      resp = "server data"
    msgutil.send_message(request, resp.decode('utf-8'))
    time.sleep(5)
    msgutil.close_connection(request)
    time.sleep(5)
  elif request.ws_protocol == "test 20":
    msgutil.send_message(request, "server data")
    msgutil.close_connection(request)
  while not request.client_terminated:
    msgutil.receive_message(request)
