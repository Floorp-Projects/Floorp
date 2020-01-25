// This is a derived version of simple/pipeline/server.rs from
// tokio_proto crate used under MIT license.
//
// Original version of server.rs:
// https://github.com/tokio-rs/tokio-proto/commit/8fb8e482dcd55cf02ceee165f8e08eee799c96d3
//
// The following modifications were made:
// * Simplify the code to implement RPC for pipeline requests that
//   contain simple request/response messages:
//   * Remove `Error` types,
//   * Remove `bind_transport` fn & `BindTransport` type,
//   * Remove all "Lift"ing functionality.
//   * Remove `Service` trait since audioipc doesn't use `tokio_service`
//     crate.
//
// Copyright (c) 2016 Tokio contributors
//
// Permission is hereby granted, free of charge, to any
// person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the
// Software without restriction, including without
// limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following
// conditions:
//
// The above copyright notice and this permission notice
// shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
// ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
// PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

use crate::rpc::driver::Driver;
use crate::rpc::Handler;
use futures::{Async, Future, Poll, Sink, Stream};
use std::collections::VecDeque;
use std::io;
use tokio::runtime::current_thread;

/// Bind an async I/O object `io` to the `server`.
pub fn bind_server<S>(transport: S::Transport, server: S)
where
    S: Server,
{
    let fut = {
        let handler = ServerHandler {
            server,
            transport,
            in_flight: VecDeque::with_capacity(32),
        };
        Driver::new(handler)
    };

    // Spawn the RPC driver into task
    current_thread::spawn(fut.map_err(|_| ()))
}

pub trait Server: 'static {
    /// Request
    type Request: 'static;

    /// Response
    type Response: 'static;

    /// Future
    type Future: Future<Item = Self::Response, Error = ()>;

    /// The message transport, which works with async I/O objects of
    /// type `A`.
    type Transport: 'static
        + Stream<Item = Self::Request, Error = io::Error>
        + Sink<SinkItem = Self::Response, SinkError = io::Error>;

    /// Process the request and return the response asynchronously.
    fn process(&mut self, req: Self::Request) -> Self::Future;
}

////////////////////////////////////////////////////////////////////////////////

struct ServerHandler<S>
where
    S: Server,
{
    // The service handling the connection
    server: S,
    // The transport responsible for sending/receving messages over the wire
    transport: S::Transport,
    // FIFO of "in flight" responses to requests.
    in_flight: VecDeque<InFlight<S::Future>>,
}

impl<S> Handler for ServerHandler<S>
where
    S: Server,
{
    type In = S::Request;
    type Out = S::Response;
    type Transport = S::Transport;

    /// Mutable reference to the transport
    fn transport(&mut self) -> &mut Self::Transport {
        &mut self.transport
    }

    /// Consume a message
    fn consume(&mut self, request: Self::In) -> io::Result<()> {
        trace!("ServerHandler::consume");
        let response = self.server.process(request);
        self.in_flight.push_back(InFlight::Active(response));

        // TODO: Should the error be handled differently?
        Ok(())
    }

    /// Produce a message
    fn produce(&mut self) -> Poll<Option<Self::Out>, io::Error> {
        trace!("ServerHandler::produce");

        // Make progress on pending responses
        for pending in &mut self.in_flight {
            pending.poll();
        }

        // Is the head of the queue ready?
        match self.in_flight.front() {
            Some(&InFlight::Done(_)) => {}
            _ => {
                trace!("  --> not ready");
                return Ok(Async::NotReady);
            }
        }

        // Return the ready response
        match self.in_flight.pop_front() {
            Some(InFlight::Done(res)) => {
                trace!("  --> received response");
                Ok(Async::Ready(Some(res)))
            }
            _ => panic!(),
        }
    }

    /// RPC currently in flight
    fn has_in_flight(&self) -> bool {
        !self.in_flight.is_empty()
    }
}

////////////////////////////////////////////////////////////////////////////////

enum InFlight<F: Future<Error = ()>> {
    Active(F),
    Done(F::Item),
}

impl<F: Future<Error = ()>> InFlight<F> {
    fn poll(&mut self) {
        let res = match *self {
            InFlight::Active(ref mut f) => match f.poll() {
                Ok(Async::Ready(e)) => e,
                Err(_) => unreachable!(),
                Ok(Async::NotReady) => return,
            },
            _ => return,
        };
        *self = InFlight::Done(res);
    }
}
