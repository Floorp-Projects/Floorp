/* MPL-2.0 */

export type HttpMethod = "GET" | "POST" | "DELETE" | "OPTIONS";

export interface RouteMatch {
  handler: Handler<any, any>;
  params: Record<string, string>;
}

export interface Context<J = unknown> {
  method: HttpMethod;
  pathname: string;
  searchParams: URLSearchParams;
  headers: Record<string, string>;
  body: Uint8Array;
  // Path parameters captured from the route pattern, e.g. "/items/:id"
  params: Record<string, string>;
  // Convenience helpers injected by server
  json<T = J>(): T | null;
}

export interface StreamResult {
  isStream: true;
  onConnect: (
    send: (data: string) => void,
    close: () => void,
  ) => (() => void) | void;
}

export interface HttpResult<T = unknown> {
  status?: number;
  body?: T;
}

export type Handler<Req = unknown, Res = unknown> = (
  ctx: Context<Req>,
) => Promise<HttpResult<Res> | StreamResult> | HttpResult<Res> | StreamResult;

interface CompiledRoute {
  method: HttpMethod;
  pattern: string;
  regex: RegExp;
  paramNames: string[];
  handler: Handler<any, any>;
}

export class Router {
  private routes: CompiledRoute[] = [];

  register<Req = unknown, Res = unknown>(
    method: HttpMethod,
    pattern: string,
    handler: Handler<Req, Res>,
  ) {
    const { regex, paramNames } = compilePattern(pattern);
    this.routes.push({
      method,
      pattern,
      regex,
      paramNames,
      handler: handler as unknown as Handler<any, any>,
    });
  }

  match(method: string, pathname: string): RouteMatch | null {
    for (const r of this.routes) {
      if (r.method !== method) continue;
      const m = r.regex.exec(pathname);
      if (!m) continue;
      const params: Record<string, string> = {};
      r.paramNames.forEach((name, i) => (params[name] = m[i + 1]));
      return { handler: r.handler, params };
    }
    return null;
  }
}

function compilePattern(pattern: string): {
  regex: RegExp;
  paramNames: string[];
} {
  const parts = pattern.split("/").filter(Boolean);
  const names: string[] = [];
  const re = parts
    .map((p) => {
      if (p.startsWith(":")) {
        names.push(p.slice(1));
        return "([^/]+)";
      }
      return escapeRegex(p);
    })
    .join("/");
  const regex = new RegExp(`^/${re}$`);
  return { regex, paramNames: names };
}

function escapeRegex(s: string) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

export class NamespaceBuilder {
  constructor(
    private router: Router,
    private base: string,
  ) {}

  namespace(seg: string, fn: (ns: NamespaceBuilder) => void) {
    const next = new NamespaceBuilder(this.router, join(this.base, seg));
    fn(next);
    return this;
  }

  get<Req = unknown, Res = unknown>(seg: string, handler: Handler<Req, Res>) {
    this.router.register("GET", join(this.base, seg), handler);
    return this;
  }
  post<Req = unknown, Res = unknown>(seg: string, handler: Handler<Req, Res>) {
    this.router.register("POST", join(this.base, seg), handler);
    return this;
  }
  delete<Req = unknown, Res = unknown>(
    seg: string,
    handler: Handler<Req, Res>,
  ) {
    this.router.register("DELETE", join(this.base, seg), handler);
    return this;
  }
}

export function createApi(router: Router) {
  return new NamespaceBuilder(router, "");
}

function join(base: string, seg: string) {
  const b = base.endsWith("/") ? base.slice(0, -1) : base;
  const s = seg ? (seg.startsWith("/") ? seg : `/${seg}`) : "";
  return (b || "") + s;
}
