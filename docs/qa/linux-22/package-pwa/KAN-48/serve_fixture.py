#!/usr/bin/env python3
import http.server
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from pwa_ui_probe import Fixture
http.server.ThreadingHTTPServer(('127.0.0.1',8775),Fixture).serve_forever()
