from mod_pywebsocket import msgutil
from mod_pywebsocket import common

def web_socket_do_extra_handshake(request):
  deflate_found = False

  if request.ws_extension_processors is not None:
    for extension_processor in request.ws_extension_processors:
      if extension_processor.name() == "deflate":
        extension_processor.set_client_no_context_takeover(True)
        deflate_found = True

  if deflate_found is False:
    raise ValueError('deflate extension processor not found')

def web_socket_transfer_data(request):
  while True:
    rcvd = msgutil.receive_message(request)
    opcode = request.ws_stream.get_last_received_opcode()
    if (opcode == common.OPCODE_BINARY):
      msgutil.send_message(request, rcvd, binary=True)
    elif (opcode == common.OPCODE_TEXT):
      msgutil.send_message(request, rcvd)
